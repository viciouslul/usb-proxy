#include "tusb.h"
#include "hardware/clocks.h"
#include "bsp/board_api.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "proxy_host.h"
#include "proxy_device.h"

#define SELECT_BUTTON       27 //A1
#define SCROLL_BUTTON       28 //A2
#define LED_PIN             11

void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == SCROLL_BUTTON)
    {
        current_screen = (current_screen + 1) % MENU_COUNT;
        update_display = true;
    }

    if (gpio == SELECT_BUTTON)
    {
        //handle select
        device_selected = current_screen;
    }
}

void core1_main()
{
    if (!device_selected) return;
    //host task only when we have selected a device
    host_task();
}

/*------------- MAIN -------------*/
int main(void)
{
    /*GPIO SETUP*/
    gpio_init(LED_PIN);
    gpio_init(SELECT_BUTTON);
    gpio_init(SCROLL_BUTTON);

    //enter init
    gpio_set_dir(SELECT_BUTTON, GPIO_IN);
    gpio_pull_up(SELECT_BUTTON);
    gpio_set_irq_enabled_with_callback(SELECT_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    //scroll init
    gpio_set_dir(SCROLL_BUTTON, GPIO_IN);
    gpio_pull_up(SCROLL_BUTTON);
    gpio_set_irq_enabled_with_callback(SCROLL_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    //LED
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // init device and host stack on configured roothub port
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    tusb_rhport_init_t host_init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO};
    tusb_init(BOARD_TUH_RHPORT, &host_init);

    board_init_after_tusb();

    multicore_launch_core1(core1_main);

    device_task();
}