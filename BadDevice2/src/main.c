#include <stdio.h>
#include "device_core.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "usb_descriptors.h"
#include "device_core.h"
#include "keymap.h"
#include "neopixel.h"
#include <stdlib.h>
#include "hardware/uart.h"

#define LED_PIN           11
#define NEOPIXEL_PIN      12
#define BT_UART           uart0
#define BT_BAUD_RATE      9600
#define BT_TX_PIN         0
#define BT_RX_PIN         1

/* How long a key's LED stays white after being pressed (ms) */
#define KEY_LIGHT_MS      100

/* LED update rate — neopixel_show() blocks ~330 µs so don't call every loop */
#define LED_UPDATE_MS     10

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

/* Set in ISR, handled in main loop (reEnumerate calls sleep_ms - not ISR-safe) */
static volatile bool reenumerate_pending = false;
static volatile bool payload_pending     = false;

void handleUartRx(void)
{
    while (uart_is_readable(BT_UART))
    {
        char cmd = uart_getc(BT_UART);
        if (cmd == '1')
            payload_pending = true;
        else if (cmd == '2')
            reenumerate_pending = true;
    }
}

int main(void)
{
    /* USB setup */
    board_init();
    srand(board_millis());
    tud_init(BOARD_TUD_RHPORT);
    if (board_init_after_tusb)
    {
        board_init_after_tusb();
    }
    sleep_ms(500);

    /* GPIO setup */
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /* Key GPIO setup - active-low, internal pull-up */
    for (int i = 0; i < KEY_COUNT; i++)
    {
        gpio_init(key_pins[i]);
        gpio_set_dir(key_pins[i], GPIO_IN);
        gpio_pull_up(key_pins[i]);
    }

    /* UART setup for HC-06 Bluetooth module */
    uart_init(BT_UART, BT_BAUD_RATE);
    gpio_set_function(BT_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(BT_RX_PIN, GPIO_FUNC_UART);

    irq_set_exclusive_handler(UART0_IRQ, handleUartRx);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(BT_UART, true, false);

    /* NeoPixel setup — chain starts at NEOPIXEL_PIN */
    neopixel_init(NEOPIXEL_PIN);
    neopixel_fill(0, 0, 20); /* Dim blue = macropad idle */
    neopixel_show();

    /* Key state tracking */
    bool     key_prev[KEY_COUNT]       = {0};
    uint32_t key_last_time[KEY_COUNT]  = {0};
    uint32_t key_light_time[KEY_COUNT] = {0};
    uint32_t led_last_update           = 0;

    while (1)
    {
        if (payload_pending)
        {
            payload_pending = false;
            fillPayload("u got rekt\n", false);
        }

        if (reenumerate_pending)
        {
            reenumerate_pending = false;
            if (current_dev == MSC)
                reEnumerate(HID);
            else
                reEnumerate(MSC);
        }

        /* Macropad key scanning */
        uint32_t now = board_millis();
        for (int i = 0; i < KEY_COUNT; i++)
        {
            bool pressed = !gpio_get(key_pins[i]); /* active-low */
            if (pressed && !key_prev[i] && (now - key_last_time[i]) >= KEY_DEBOUNCE_MS)
            {
                sendKey(keymap[i].modifier, keymap[i].keycode);
                key_last_time[i]  = now;
                key_light_time[i] = now; /* start white burst */
            }
            key_prev[i] = pressed;
        }

        /* NeoPixel update (throttled) */
        if ((board_millis() - led_last_update) >= LED_UPDATE_MS)
        {
            if (payload_pending || !hid_is_idle())
            {
                /* Red = payload running */
                neopixel_fill(40, 0, 0);
            }
            else
            {
                /* Blue idle, white burst on recently-pressed keys */
                uint32_t t = board_millis();
                for (int i = 0; i < KEY_COUNT; i++)
                {
                    if ((t - key_light_time[i]) < KEY_LIGHT_MS)
                        neopixel_set(i, 40, 40, 40); /* white burst */
                    else
                        neopixel_set(i, 0, 0, 20);   /* dim blue */
                }
            }
            neopixel_show();
            led_last_update = board_millis();
        }

        tud_task();
        hidTask();
    }
}
