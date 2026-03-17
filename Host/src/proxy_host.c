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

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    // 1. Send the mount packet to your proxy queue
    proxy_packet_t pkt;
    pkt.hid_msg = PROXY_MSG_MOUNT;
    pkt.hid_device = get_device_type(itf_protocol);
    pkt.timestamp_us = time_us_32();
    pkt.report_len = 0; 
    (void) proxy_enqueue(&pkt);

    #ifdef PROXY_DEBUG
    const char* protocol_str[] = {"None", "Keyboard", "Mouse"};
    char tempbuf[256];
    // Note: ensure itf_protocol doesn't exceed index 2
    int count = sprintf(
        tempbuf, "[%04x:%04x][%u] HID Interface%u, Protocol = %s\r\n", 
        vid, pid, dev_addr, instance, protocol_str[itf_protocol <= 2 ? itf_protocol : 0]);
    tud_cdc_write(tempbuf, (uint32_t) count);
    tud_cdc_write_flush();
    #endif

    // 2. FORCE Boot Protocol if it's a keyboard or mouse
    // This makes sure the device sends the standard 8-byte format your code expects
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || itf_protocol == HID_ITF_PROTOCOL_MOUSE) 
    {
        tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);
    }

    // 3. Request the first report
    // FIXED: Explicitly checking each condition so HID_ITF_PROTOCOL_NONE (0) is included
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || 
        itf_protocol == HID_ITF_PROTOCOL_MOUSE    || 
        itf_protocol == HID_ITF_PROTOCOL_NONE)
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
    
    // Default to using the raw report
    uint8_t const* p_report = report;
    uint16_t r_len = len;

    // SHIFT FIX: If the device is using Report IDs (len 9), 
    // the actual keyboard data starts at index [1]
    if (len == 9) {
        p_report = report + 1;
        r_len = len - 1;
    }

    proxy_device_t dtype = get_device_type(itf_protocol);
    if (dtype == HID_NONE) dtype = HID_KEYBOARD; 

    // Debug output using the shifted report
    if (dtype == HID_KEYBOARD) {
        #ifdef PROXY_DEBUG
        debug_kbd_report(dev_addr, (hid_keyboard_report_t const*) p_report);
        #endif
    }

    // Queue the shifted data
    proxy_packet_t pkt;
    pkt.hid_msg = PROXY_MSG_REPORT;
    pkt.hid_device = dtype;
    pkt.timestamp_us = time_us_32();
    pkt.report_len = r_len;
    memcpy(pkt.report, p_report, r_len);
    (void) proxy_enqueue(&pkt);

    // Keep the engine running
    tuh_hid_receive_report(dev_addr, instance);
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