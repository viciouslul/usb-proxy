    #include <stdio.h>
    #include "pico/stdlib.h"
    #include "device_core.h"
    #include "pico/stdlib.h"
    #include "tusb.h"
    #include "bsp/board_api.h"
    #include "usb_descriptors.h"

    #define LED_PIN 11
    #define GPIO_BTN_1 26
    #define GPIO_BTN_2 27

    // Required TinyUSB HID callbacks
    uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                    hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
        return 0;
    }

    void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {}

    void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
    {
        (void)instance;
        (void)report;
        (void)len;
    }
    //invoked when device is mounted
    void tud_mount_cb(void)
    {
        gpio_put(LED_PIN, 1);
    }

    // Invoked when device is unmounted
    void tud_umount_cb(void)
    {
        gpio_put(LED_PIN, 0);
    }

    void tud_suspend_cb(bool remote_wakeup_en)
    {
    (void)remote_wakeup_en;
    }

    // Invoked when usb bus is resumed
    void tud_resume_cb(void)
    {
        //
    }

    void gpio_callback(uint gpio, uint32_t events)
    {
        if(gpio == GPIO_BTN_1)
        {
            gpio_put(LED_PIN, 0);
        }
        else if(gpio == GPIO_BTN_2)
        {
            //do something else
        }
    }

    int main()
    {
        /*USB SETUP*/
        board_init();
        tud_init(BOARD_TUD_RHPORT);
        if (board_init_after_tusb)
        {
            board_init_after_tusb();
        }
        sleep_ms(500);

        /*GPIO SETUP*/
        gpio_init(LED_PIN);
        gpio_init(GPIO_BTN_1);
        gpio_set_dir(GPIO_BTN_1, GPIO_IN);
        gpio_pull_up(GPIO_BTN_1);
        gpio_set_irq_enabled_with_callback(GPIO_BTN_1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
        gpio_init(GPIO_BTN_2);
        gpio_set_dir(GPIO_BTN_2, GPIO_IN);
        gpio_pull_up(GPIO_BTN_2);
        gpio_set_irq_enabled_with_callback(GPIO_BTN_2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
        gpio_set_dir(LED_PIN, GPIO_OUT);

        while (1)
        {
            tud_task();
            hid_task(); //device loop
        }
    }
