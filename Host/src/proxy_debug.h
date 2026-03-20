#pragma once
#include "tusb.h"

// convert hid keycode to ascii and print via usb device CDC (ignore non-printable)
static void debug_kbd_report(uint8_t dev_addr, hid_keyboard_report_t const* report)
{
    (void) dev_addr;
#ifdef PROXY_DEBUG
    static hid_keyboard_report_t prev_report = {0, 0, {0}}; // previous report to check key released
    bool flush = false;

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t keycode = report->keycode[i];
        if (keycode)
        {
            bool found_key_in_prev_report = false;
            for (uint8_t i = 0; i < 6; i++)
            {
                if (prev_report.keycode[i] == keycode)
                    found_key_in_prev_report = true;
            }
            if (found_key_in_prev_report)
            {
                // exist in previous report means the current key is holding
            } else
            {
                // not existed in previous report means the current key is pressed
                bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
                static uint8_t const keycode2ascii[128][2] = {HID_KEYCODE_TO_ASCII};
                uint8_t ch = keycode2ascii[keycode][is_shift ? 1 : 0];

                if (ch)
                {
                    if (ch == '\n') tud_cdc_write("\r", 1);
                    tud_cdc_write(&ch, 1);
                    flush = true;
                }
            }
        }
        // TODO example skips key released
    }

    if (flush)
    {
        tud_cdc_write_flush();
    }

    prev_report = *report;
#else
    (void) report;
#endif // Proxy debug
}

// send mouse report to usb device CDC
static void debug_mouse_report(uint8_t dev_addr, hid_mouse_report_t const* report)
{
#ifdef PROXY_DEBUG
    //------------- button state  -------------//
    //uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
    char l = report->buttons & MOUSE_BUTTON_LEFT ? 'L' : '-';
    char m = report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-';
    char r = report->buttons & MOUSE_BUTTON_RIGHT ? 'R' : '-';

    char tempbuf[32];
    int count = sprintf(tempbuf, "[%u] %c%c%c %d %d %d\r\n", dev_addr, l, m, r, report->x, report->y, report->wheel);


    tud_cdc_write(tempbuf, (uint32_t) count);
    tud_cdc_write_flush();
#else
    (void) dev_addr;
    (void) report;
#endif
}
