#include <stdio.h>
#include "device_core.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "usb_descriptors.h"
#include "device_core.h"
#include <stdlib.h>
#include "hardware/uart.h"

#define LED_PIN           11
#define BT_UART           uart0
#define BT_BAUD_RATE      9600
#define BT_TX_PIN         0
#define BT_RX_PIN         1

/* Required TinyUSB callbacks */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
    (void) instance;
    (void) report;
    (void) len;
}

/* Invoked when device is mounted */
void tud_mount_cb(void)
{
    gpio_put(LED_PIN, 1);
}

/* Invoked when device is unmounted */
void tud_umount_cb(void)
{
    gpio_put(LED_PIN, 0);
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
}

/* Invoked when USB bus is resumed */
void tud_resume_cb(void)
{
    /* Do nothing */
}


int main(void)
{
    /* USB setup */
    board_init();
    srand(board_millis());  /* Seed random number generator */
    tud_init(BOARD_TUD_RHPORT);
    if (board_init_after_tusb) 
    {
        board_init_after_tusb();
    }
    sleep_ms(500);

    /* GPIO setup */
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /* UART setup for HC-06 Bluetooth module */
    uart_init(BT_UART, BT_BAUD_RATE);
    gpio_set_function(BT_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(BT_RX_PIN, GPIO_FUNC_UART);

    while (1) 
    {
        while (uart_is_readable(BT_UART)) 
        {
            char cmd = uart_getc(BT_UART);
            if (cmd == '1') 
            {
                fillPayload("u got rekt\n", true); /* Linux */
            } 
            else if (cmd == '2') 
            {
                if (current_dev == MSC) 
                    reEnumerate(HID);
                else 
                    reEnumerate(MSC);
            }
        }

        tud_task();
        hid_task(); /* Device loop */
    }
}