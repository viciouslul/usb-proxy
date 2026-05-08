#include "proxy_detection.h"
#include "proxy_metrics.h"
#include <stdlib.h>

static uint32_t kb_last           = 0;
static uint32_t mouse_last        = 0;
static uint8_t  kb_prev_keycode   = 0x00;
static uint8_t  kb_prev_report[8] = {0};
static uint32_t kb_history[4]     = {0};
static uint8_t  kb_history_idx    = 0;
static uint8_t  strike_count      = 0;

static void mouseDetection(uint8_t *mouse_data);
static void keyboardDetection(uint8_t *keyboard_data, uint32_t time);
static void packageValidation(uint8_t *report, uint16_t report_len, proxy_device_t dev_t);

static bool bot_detected = false;

bool filterPacket(proxy_packet_t *pkt)
{
    if (pkt->msg_t == PROXY_MSG_REPORT)
    {
        if (pkt->dev_t == HID_KEYBOARD)
        {
            packageValidation(pkt->report, pkt->report_len, pkt->dev_t);
            keyboardDetection(pkt->report, pkt->timestamp_us);
        }
        else if (pkt->dev_t == HID_MOUSE)
        {
            packageValidation(pkt->report, pkt->report_len, pkt->dev_t);
            mouseDetection(pkt->report);
        }
    }

    return bot_detected;
}

static void mouseDetection(uint8_t *mouse_data)
{
    // NOT IMPLEMENTED
    (void)mouse_data;
}

static void keyboardDetection(uint8_t *keyboard_data, uint32_t time)
{
    // rollover guard
    for (int i = 2; i < 8; i++)
    {
        if (keyboard_data[i] == 0x01)
            return;
    }

    // 1. Check if the report is actually different from the last one
    // and if any key is actually being held down.
    // check if old key was present, we only wanna detect if new one comes in
    bool is_new_report = memcmp(kb_prev_report, keyboard_data, 8) != 0;
    if (!is_new_report)
        return;

    bool any_key_down = false;
    bool new_key      = false;

    for (int i = 2; i < 8; i++)
    {
        if (keyboard_data[i] != 0) // is any key pressed
        {
            any_key_down = true;
            uint8_t key  = keyboard_data[i]; // check the key

            bool already_pressed = false;
            for (int j = 2; j < 8; j++) // check if this key was present in the previous report
            {
                if (kb_prev_report[j] == key)
                {
                    already_pressed = true;
                    break;
                }
            }

            if (!already_pressed)
            {
                new_key = true;
                break;
            }
        }
    }

    // 2. Only process if it's a fresh key press event
    if (new_key && any_key_down)
    {
        uint8_t current_keycode = keyboard_data[2]; // Get the new key
        if (kb_last != 0)
        {
            uint32_t current_interval = time - kb_last;

            // Record keystroke interval (not marked suspicious yet)
            metrics_record_keystroke(current_interval, current_keycode, false);

            // Store the delay
            kb_history[kb_history_idx] = current_interval;
            kb_history_idx             = (kb_history_idx + 1) % 4;

            // Only analyze if we have at least 4 samples
            if (kb_history[3] != 0)
            {
                // calculate mean
                uint32_t sum = 0;
                for (int i = 0; i < 4; i++)
                    sum += kb_history[i];
                uint32_t avg = sum / 4;

                // calculate spread (absolute deviation)
                uint32_t spread = 0;
                for (int i = 0; i < 4; i++)
                {
                    spread += abs((int32_t)kb_history[i] - (int32_t)avg);
                }

                if (avg < BOT_AVG_THRESHOLD && spread < MIN_SPREAD_THRESHOLD)
                {
                    strike_count++;
                    // Mark recent keystrokes as suspicious and trigger bot alert
                    for (int i = 0; i < 4; i++)
                    {
                        metrics_record_keystroke(kb_history[i], current_keycode, true);
                    }
                    metrics_record_event((detection_event_t){.device_type = HID_KEYBOARD,
                                                             .event_type  = EVENT_STRIKE});
                    memset(kb_history, 0, sizeof(kb_history));
                    if (strike_count >= 4)
                    {
                        bot_detected = true;
                    }
                }
            }
        }
        kb_last = time;
    }

    // reset strike_count after a few seconds
    if ((time - kb_last) >= STRIKE_TIMEOUT_US)
        strike_count = 0;

    // Always update the previous report for the next comparison
    memcpy(kb_prev_report, keyboard_data, 8);
}

void pktFilterReset(void)
{
    kb_last         = 0;
    mouse_last      = 0;
    kb_prev_keycode = 0x00;
    kb_history_idx  = 0;
    strike_count    = 0;
    bot_detected    = false;
    memset(kb_prev_report, 0, sizeof(kb_prev_report));
    memset(kb_history, 0, sizeof(kb_history));
}

enumeration_result_t enumerationCheck(proxy_packet_t *pkt,
                                      proxy_device_t  selected_device)
{
    if (pkt->msg_t == PROXY_MSG_MOUNT)
    {
        switch (pkt->dev_t)
        {
        case HID_KEYBOARD:
            return (selected_device == HID_KEYBOARD) ? ENUM_CHECK_OK
                                                     : ENUM_CHECK_ERROR;

        case HID_MOUSE:
            return (selected_device == HID_MOUSE) ? ENUM_CHECK_OK : ENUM_CHECK_ERROR;

        case MSC:
            return (selected_device == MSC) ? ENUM_CHECK_OK : ENUM_CHECK_ERROR;

        default:
            return ENUM_CHECK_ERROR;
        }
    }
    else if (pkt->msg_t == PROXY_MSG_UNMOUNT)
    {
        return ENUM_CHECK_UNMOUNT;
    }

    return ENUM_CHECK_NONE;
}

static void packageValidation(uint8_t *report, uint16_t report_len, proxy_device_t dev_t)
{
    // Empty reports are stale IN-completions emitted by the host stack during
    // disconnect — they carry no payload and are not a bot signature.
    if (report_len == 0)
        return;

    switch (dev_t)
    {
    case HID_KEYBOARD:
        if (report_len != 8)
        {
            bot_detected = true;
            return;
        }
        // Reserved byte must be 0x00 per HID boot protocol spec
        if (report[1] != 0x00)
        {
            bot_detected = true;
            return;
        }
        // Keycodes above 0xE7 are not defined in HID usage tables
        for (int i = 2; i < 8; i++)
        {
            if (report[i] != 0x00 && report[i] > 0xE7)
            {
                bot_detected = true;
                return;
            }
        }
        break;

    case HID_MOUSE:
        // we are assuming its a "normal" mouse with m1, m2, scroll, sensor.
        if (report_len < 3 || report_len > 4)
        {
            bot_detected = true;
            return;
        }
        // Only bits 0-2 are valid (left, right, middle); bits 3-7 should never be set
        if (report[0] & 0xF8)
        {
            bot_detected = true;
            return;
        }
        break;

    default:
        break;
    }
}
