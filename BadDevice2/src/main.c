#include <stdio.h>
#include "device_core.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "usb_descriptors.h"
#include "device_core.h"

#define LED_PIN 11
#define GPIO_BTN_1 26
#define GPIO_BTN_2 27
#define DEBOUNCE_MS 150

/*Globals*/
volatile uint32_t last_key_btn1 = 0;
volatile uint32_t last_key_btn2 = 0;
volatile uint8_t handle_btn1 = 0;
volatile uint8_t handle_btn2 = 0;

/*Required tinyusb callbacks*/
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
    //do something
}

void gpio_callback(uint gpio, uint32_t events)
{
    uint32_t now = board_millis();
    if(gpio == GPIO_BTN_1)
    {
        if((now - last_key_btn1) >= DEBOUNCE_MS)
        {
            handle_btn1 = 1;
            last_key_btn1 = now;
        }
    }
    else if(gpio == GPIO_BTN_2)
    {
        if((now - last_key_btn2) >= DEBOUNCE_MS)
        {
            handle_btn2 = 1;
            last_key_btn2 = now;
        }
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
    gpio_set_irq_enabled_with_callback(GPIO_BTN_1, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_init(GPIO_BTN_2);
    gpio_set_dir(GPIO_BTN_2, GPIO_IN);
    gpio_pull_up(GPIO_BTN_2);
    gpio_set_irq_enabled_with_callback(GPIO_BTN_2, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (1)
    {
        if (handle_btn1)
        {
            fillPayload("u got rekt\n"); //linux
            handle_btn1 = 0;
        }
        if (handle_btn2)
        {
            if (current_dev == MSC)
            {
                reEnumerate(HID);
            }
            else
            {
                reEnumerate(MSC);
            }
            handle_btn2 = 0;
        }

        tud_task();
        hid_task(); //device loop
    }
}