#include "proxy_device.h"
#include "tusb.h"

void device_task()
{
    while (1)
    {
        tud_task(); // tinyusb device task
        process_hid();
    }
}
/* Functions */
void process_hid()
{
    proxy_packet_t pkt;

    while(proxy_dequeue(&pkt))
    {
        //botDetection(&pkt);
        switch(pkt.msg_t)
        {
            case PROXY_MSG_REPORT:
                if(pkt.dev_t == HID_KEYBOARD)
                {
                    tud_hid_report(pkt.dev_t, pkt.report, pkt.report_len);
                    #ifdef PROXY_DEBUG
                    if (pkt.report[2] != 0x00)
                    {
                        // collect_kb_delay(pkt.timestamp_us, time_us_32());
                    }
                    #endif
                }
                else if(pkt.dev_t == HID_MOUSE)
                {
                    tud_hid_report(pkt.dev_t, pkt.report, pkt.report_len);
                }
                break;

            case PROXY_MSG_MOUNT:
                //handle mount
                break;

            case PROXY_MSG_UNMOUNT:
                //handle unmount
                break;

            case PROXY_MSG_NONE:
                //do nothing
                break;

            default:
                break;
        }
    }
}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
    (void)instance;
    (void)report;
    (void)len;
}