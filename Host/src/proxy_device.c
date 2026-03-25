#include "proxy_device.h"
#include "tusb.h"
#include "proxy_ssd1306.h"

void device_task()
{
    while (1)
    {
        tud_task(); // tinyusb device task
        process_hid();
        display_task();
    }
}
/* Functions */
void process_hid()
{
    proxy_packet_t pkt;

    while(proxy_dequeue(&pkt))
    {
        if (device_selected == NONE) return;
        enumerationCheck(&pkt);
        botDetection(&pkt);
        if (current_screen == SCREEN_ERROR) return;
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
                tud_hid_report(pkt.dev_t, pkt.report, pkt.report_len);
                break;

            case PROXY_MSG_UNMOUNT:
                botDetection_reset(); //just reset everything in detection when we unmount
                proxy_queue_reset(); //also just reset the queue
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


void display_task()
{
    if (update_display)
    {
        switch (current_screen)
        {
            case SCREEN_KEYBOARD:
                lcd_write_line(&lcd, 0, "Select Device:");
                lcd_write_line(&lcd, 1, "Keyboard");
                lcd_show(&lcd);
                update_display = false;
                break;

            case SCREEN_MOUSE:
                lcd_write_line(&lcd, 0, "Select Device:");
                lcd_write_line(&lcd, 1, "Mouse");
                lcd_show(&lcd);
                update_display = false;
                break;

            case SCREEN_MSC:
                lcd_write_line(&lcd, 0, "Select Device:");
                lcd_write_line(&lcd, 1, "MSC");
                lcd_show(&lcd);
                update_display = false;
                break;

            case SCREEN_OK:
                lcd_write_line(&lcd, 0, "Enumeration");
                lcd_write_line(&lcd, 1, "of device OK");
                lcd_show(&lcd);
                update_display = false;
                break;

            case SCREEN_ERROR:
                //draw error and do lock the device here
                device_selected = NONE;
                lcd_write_line(&lcd, 1, "ERROR");
                lcd_show(&lcd);
                sleep_ms(5000);
                current_screen = SCREEN_KEYBOARD;
                update_display = true;
                break;

            default:
                //draw screen welcome or something
                break;
        }
    }
}