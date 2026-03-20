#include "device_core.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "usb_descriptors.h"
#include <string.h>
#include "stdlib.h"
#include "hardware/structs/usb.h"

uint8_t const ascii_to_keycode[128][2] = { HID_ASCII_TO_KEYCODE };

void fillPayload(const char* inputPayload)
{
    payload = inputPayload;
    payload_index = 0;
    payload_len = strlen(inputPayload);
    sent = false;
    state = STATE_KEY_DOWN;
}

void reEnumerate(mounted_dev new_type)
{
    tud_disconnect();
    sleep_ms(1000);

    switch(new_type)
    {
        case MSC:
            current_dev = MSC;
            desc_device.idVendor  = USB_VID_MSC;
            desc_device.idProduct = USB_PID_MSC;

            string_desc_arr[1] = "TinyUSB";
            string_desc_arr[2] = "BAD MSC";
            string_desc_arr[3] = "1337";

            active_config = desc_configuration_msc;
            break;

        case HID:
            current_dev = HID;
            desc_device.idVendor  = USB_VID_HID;
            desc_device.idProduct = USB_PID_HID;

            string_desc_arr[1] = "TinyUSB";
            string_desc_arr[2] = "BAD HID";
            string_desc_arr[3] = "0420";

            active_config = desc_configuration_hid;
            break;
    }

    sleep_ms(500);
    tud_connect();
}

void hid_task(void)
{
    if (current_dev == MSC) return;
    if (!tud_mounted()) return;
    if (sent) return;

    // enforce small delay between USB actions (10 ms)
    if (board_millis() - last_event < 10) return;

    if (!tud_hid_ready()) return;

    static hid_keyboard_report_t report;

    switch (state)
    {
        case STATE_IDLE:
            return;

        case STATE_KEY_DOWN:
        {
            if (payload_index >= payload_len)
            {
                sent = true;
                state = STATE_IDLE;
                return;
            }

            uint8_t c = payload[payload_index];

            memset(&report, 0, sizeof(report));

            if (c < 128)
            {
                report.modifier = ascii_to_keycode[c][0];
                report.keycode[0] = ascii_to_keycode[c][1];
            }

            tud_hid_keyboard_report(REPORT_ID_KEYBOARD,
                                    report.modifier,
                                    report.keycode);

            state = STATE_KEY_UP;
            last_event = board_millis();
        }
        break;

        case STATE_KEY_UP:
        {
            // release key
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);

            state = STATE_DELAY;
            last_event = board_millis();
        }
        break;

        case STATE_DELAY:
        {
            payload_index++;
            state = STATE_KEY_DOWN;
        }
        break;
    }
}
