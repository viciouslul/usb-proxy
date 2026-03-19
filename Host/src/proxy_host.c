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

    // Default to use raw report
    uint8_t const *p_report = report;
    uint16_t r_len = len;

    if (r_len > 8)
    {
        p_report = report + 1;
        r_len = r_len - 1;
    }

    proxy_device_t hid_type = get_device_type(itf_protocol);


#ifdef PROXY_DEBUG
    if (hid_type == HID_KEYBOARD)
        debug_kbd_report(dev_addr, (hid_keyboard_report_t const*) p_report);
    if (hid_type == HID_MOUSE)
        debug_mouse_report(dev_addr, (hid_mouse_report_t const*) p_report);
#endif

    proxy_packet_t pkt;
    pkt.hid_msg = PROXY_MSG_REPORT;
    pkt.hid_device = hid_type;
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

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
static scsi_inquiry_resp_t inquiry_resp;

static bool inquiry_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data) {
  msc_cbw_t const* cbw = cb_data->cbw;
  msc_csw_t const* csw = cb_data->csw;

  if (csw->status != 0) 
  {
    #ifdef PROXY_DEBUG
    tud_cdc_write_str("Inquiry failed\r\n");
    #endif
    return false;
  }

  // Print out Vendor ID, Product ID and Rev
  #ifdef PROXY_DEBUG
  printf("%.8s %.16s rev %.4s\r\n", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);
  #endif

  // Get capacity of device
  uint32_t const block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
  uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

  #ifdef PROXY_DEBUG
  printf("Disk Size: %" PRIu32 " MB\r\n", block_count / ((1024*1024)/block_size));
  printf("Block Count = %" PRIu32 ", Block Size: %" PRIu32 "\r\n", block_count, block_size);
  #endif

  return true;
}

//------------- IMPLEMENTATION -------------//
void tuh_msc_mount_cb(uint8_t dev_addr) 
{
#ifdef PROXY_DEBUG
  tud_cdc_write_str("A MassStorage device is mounted\r\n");
#endif
  uint8_t const lun = 0;
  tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
  (void) dev_addr;
#ifdef PROXY_DEBUG
  tud_cdc_write_str("A MassStorage device is unmounted\r\n");
#endif
}