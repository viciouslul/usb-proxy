#include "proxy_host.h"
#include "proxy_debug.h"

void host_task()
{
    while (1){ tuh_task(); }
}

/*--------- HID ---------*/
proxy_device_t get_hid_type(uint8_t itf_protocol)
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
    pkt.msg_t = PROXY_MSG_MOUNT;
    pkt.dev_t = get_hid_type(itf_protocol);
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

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || itf_protocol == HID_ITF_PROTOCOL_MOUSE)
    {
        tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);
    }

    // Receive report from boot keyboard & mouse only
    // tuh_hid_report_received_cb() will be invoked when report is available
    if (!tuh_hid_receive_report(dev_addr, instance))
    {
    #ifdef PROXY_DEBUG
        tud_cdc_write_str("Error: cannot request report\r\n");
    #endif
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    proxy_packet_t pkt;
    pkt.msg_t = PROXY_MSG_UNMOUNT;
    pkt.dev_t = HID_NONE;
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

    // Default to use raw report
    uint8_t const *p_report = report;
    uint16_t r_len = len;

    if (r_len > 8)
    {
        p_report = report + 1;
        r_len = r_len - 1;
    }

    proxy_device_t hid_type = get_hid_type(itf_protocol);


#ifdef PROXY_DEBUG
    if (hid_type == HID_KEYBOARD)
        debug_kbd_report(dev_addr, (hid_keyboard_report_t const*) p_report);
    if (hid_type == HID_MOUSE)
        debug_mouse_report(dev_addr, (hid_mouse_report_t const*) p_report);
#endif

    proxy_packet_t pkt;
    pkt.msg_t = PROXY_MSG_REPORT;
    pkt.dev_t = hid_type;
    pkt.timestamp_us = time_us_32();
    pkt.report_len = r_len;
    memcpy(pkt.report, p_report, r_len);
    (void) proxy_enqueue(&pkt);

    // continue to request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance))
    {
        #ifdef PROXY_DEBUG
        tud_cdc_write_str("Error: cannot request report\r\n");
        #endif
    }
}

/*--------- MSC ---------*/
void tuh_msc_mount_cb(uint8_t dev_addr)
{
    (void) dev_addr;
#ifdef PROXY_DEBUG
    tud_cdc_write_str("MSC mounted\n\r");
#endif
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    (void) dev_addr;
#ifdef PROXY_DEBUG
    tud_cdc_write_str("MSC unmounted\n\r");
#endif
}