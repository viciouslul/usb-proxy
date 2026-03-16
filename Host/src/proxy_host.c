#include "proxy_host.h"

static inline bool find_key_in_report(hid_keyboard_report_t const* report, uint8_t keycode);
static void debug_kbd_report(uint8_t dev_addr, hid_keyboard_report_t const* report);
static void debug_mouse_report(uint8_t dev_addr, hid_mouse_report_t const* report);

void host_task()
{
    while (1)
    {
        tuh_task();
    }
}

proxy_device_t get_device_type(uint8_t itf_protocol)
{
    switch(itf_protocol)
    {
        case HID_ITF_PROTOCOL_NONE:     return HID_NONE;
        case HID_ITF_PROTOCOL_KEYBOARD: return HID_KEYBOARD;
        case HID_ITF_PROTOCOL_MOUSE:    return HID_MOUSE;
        default:                        return HID_NONE;
    }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    (void) desc_report;
    (void) desc_len;

    // Interface protocol (hid_interface_protocol_enum_t)
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    proxy_packet_t pkt;
    pkt.hid_msg = PROXY_MSG_MOUNT;
    pkt.hid_device = get_device_type(itf_protocol);
    pkt.timestamp_us = time_us_32();
    pkt.report_len = 0; /* Send pid and vid? */
    (void) proxy_enqueue(&pkt); /* Can check if successful or not */

    #ifdef PROXY_DEBUG
    const char* protocol_str[] = {"None", "Keyboard", "Mouse"};
    char tempbuf[256];
    int count = sprintf(
        tempbuf, "[%04x:%04x][%u] HID Interface%u, Protocol = %s\r\n", vid, pid, dev_addr, instance,
        protocol_str[itf_protocol]);
    tud_cdc_write(tempbuf, (uint32_t) count);
    tud_cdc_write_flush();
    #endif

    // Receive report from boot keyboard & mouse only
    // tuh_hid_report_received_cb() will be invoked when report is available
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || itf_protocol == HID_ITF_PROTOCOL_MOUSE)
    {
        if (!tuh_hid_receive_report(dev_addr, instance)) {
            #ifdef PROXY_DEBUG
            tud_cdc_write_str("Error: cannot request report\r\n");
            #endif
        }
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    proxy_packet_t pkt;
    pkt.hid_msg = PROXY_MSG_UNMOUNT;
    pkt.hid_device = HID_NONE;
    pkt.timestamp_us = time_us_32();
    pkt.report_len = 0;
    (void) proxy_enqueue(&pkt); /* Can check if successful or not */

#ifdef PROXY_DEBUG
    char tempbuf[256];
    int count = sprintf(tempbuf, "[%u] HID Interface%u is unmounted\r\n", dev_addr, instance);
    tud_cdc_write(tempbuf, (uint32_t) count);
    tud_cdc_write_flush();
#else
    (void) dev_addr;
    (void) instance;
#endif
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    proxy_packet_t pkt;
    pkt.hid_msg = PROXY_MSG_REPORT;
    pkt.hid_device = get_device_type(itf_protocol);
    pkt.timestamp_us = time_us_32();
    switch (itf_protocol)
    {
        case HID_ITF_PROTOCOL_KEYBOARD:
            pkt.report_len = len;
            memcpy(pkt.report, report, pkt.report_len);

            #ifdef PROXY_DEBUG
            debug_kbd_report(dev_addr, (hid_keyboard_report_t const*) report);
            #endif
            break;

        case HID_ITF_PROTOCOL_MOUSE:
            pkt.report_len = len;
            memcpy(pkt.report, report, pkt.report_len);

            #ifdef PROXY_DEBUG
            debug_mouse_report(dev_addr, (hid_mouse_report_t const*) report);
            #endif
            break;

        default:
            break;
    }
    (void) proxy_enqueue(&pkt);

    // continue to request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        #ifdef PROXY_DEBUG
        tud_cdc_write_str("Error: cannot request report\r\n");
        #endif
    }
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const* report, uint8_t keycode)
{
#ifdef PROXY_DEBUG
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) {
            return true;
        }
    }
#else
    (void) report;
    (void) keycode;
#endif
    return false;
}

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
            if (find_key_in_report(&prev_report, keycode))
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