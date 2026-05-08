#include "bsp/board_api.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "proxy_device.h"
#include "proxy_host.h"
#include "proxy_metrics.h"
#include "proxy_ssd1306.h"
#include "proxy_ui.h"
#include "proxy_whitelist.h"

#define SCROLL_BTN 26 // A0
#define SCROLL_GND 27 // A1
#define SELECT_GND 28 // A2
#define SELECT_BTN 29 // A3

#define SDA_PIN 2
#define SCL_PIN 3
#define LED_PIN 13

void gpio_callback(uint gpio, uint32_t events)
{
    (void)events;
    if (ui_has_selected_device())
        return;
    static volatile uint32_t last_scroll_btn = 0;

    uint32_t ms_now = board_millis();
    if (gpio == SCROLL_BTN)
    {
        if ((ms_now - last_scroll_btn) > 300)
        {
            ui_on_scroll_button();
            last_scroll_btn = ms_now;
        }
    }

    if (gpio == SELECT_BTN) // One hit button doesn't need to be debounced
    {
        ui_on_select_button();
    }
}

void global_init()
{
    // BUILTIN LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // LCD
    i2c_init(i2c1, 400000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    lcd_init(&lcd, i2c1, LCD_ADDR);
    lcd_write_line(&lcd, 0, "USB PROXY");
    lcd_write_line(&lcd, 1, "Initializing..");
    lcd_show(&lcd);
    ui_init();
    metrics_init();

    // SELECT BUTTON
    gpio_init(SELECT_BTN);
    gpio_set_dir(SELECT_BTN, GPIO_IN);
    gpio_pull_up(SELECT_BTN);
    gpio_set_irq_enabled_with_callback(SELECT_BTN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(SELECT_GND);
    gpio_set_dir(SELECT_GND, GPIO_OUT);
    gpio_put(SELECT_GND, 0);

    // SCROLL BUTTON
    gpio_init(SCROLL_BTN);
    gpio_set_dir(SCROLL_BTN, GPIO_IN);
    gpio_pull_up(SCROLL_BTN);
    gpio_set_irq_enabled_with_callback(SCROLL_BTN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(SCROLL_GND);
    gpio_set_dir(SCROLL_GND, GPIO_OUT);
    gpio_put(SCROLL_GND, 0);

    whitelist_init();
}

void core1_main()
{
    host_task();
}

/*------------- MAIN -------------*/
int main(void)
{
    set_sys_clock_khz(120000, true);
    board_init();

    // init device and host stack on configured roothub port
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    tusb_rhport_init_t host_init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO};
    tusb_init(BOARD_TUH_RHPORT, &host_init);

    board_init_after_tusb();

    multicore_launch_core1(core1_main);

    global_init();
    device_task();
}