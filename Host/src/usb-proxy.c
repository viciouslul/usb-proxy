#include "tusb.h"
#include "hardware/clocks.h"
#include "bsp/board_api.h"
#include "pico/multicore.h"

#include "proxy_host.h"
#include "proxy_device.h"

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

    device_task();
}