#include "tusb.h"
#include "hardware/clocks.h"
#include "bsp/board_api.h"
#include "pico/multicore.h"

#include "proxy_host.h"
#include "proxy_device.h"
#include "proxy_ssd1306.h"

void global_init()
{
    // BUILTIN LED
    gpio_init(13);
    gpio_set_dir(13, GPIO_OUT);
    gpio_put(13, 1);

    // LCD
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    lcd_init(&lcd, i2c1, LCD_ADDR);
    lcd_write_line(&lcd, 0, "USB PROXY");
    lcd_write_line(&lcd, 1, "Initializing..");
    lcd_show(&lcd);
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