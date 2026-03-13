#include "device_core.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "usb_descriptors.h"


const char *payload = "rekt";
static int index_char = 0;
static bool key_pressed = false;
static uint32_t last_ms = 0;
uint8_t const ascii_to_keycode[128][2] = { HID_ASCII_TO_KEYCODE };


void hid_task(void)
{
    static uint32_t ms = 0;
    if (board_millis() - ms < 10) return;
    ms = board_millis();

    if (!tud_mounted()) return;
    if (ms < 1000) return;

    static bool sent = false;
    if (sent) return;

    hid_keyboard_report_t report = {0};
    static bool key_down = false;
    static int i = 0;

    if (!tud_hid_ready()) return;

    if (key_down)
    {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        key_down = false;
        i++;
        return;
    }

    if (i>= (int)strlen(payload))
    {
        sent = true;
        return;
    }

    uint8_t c = payload[i];
    report.modifier = ascii_to_keycode[c][0];
    report.keycode[0] = ascii_to_keycode[c][1];
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, report.modifier, report.keycode);

    key_down = true;
}

