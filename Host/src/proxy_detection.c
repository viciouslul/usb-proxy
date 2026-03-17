#include "proxy_detection.h"

static uint32_t kb_last = 0;
static uint32_t mouse_last = 0;
static uint8_t kb_prev_keycode = 0x00;
static uint8_t kb_prev_report[8] = {0};


static void mouseDetection(uint8_t* mouse_data);
static void keyboardDetection(uint8_t* keyboard_data, uint32_t time);

void botDetection(proxy_packet_t* pkt)
{
    if(pkt->hid_msg == PROXY_MSG_REPORT)
    {
        if(pkt->hid_device == HID_KEYBOARD)
        {
            /*do some checks*/
            /*
            if(pkt->report_len != sizeof(hid_keyboard_report_t))
                while(1);

            if(pkt->report[1] != 0x00)
                while(1);
            */
            keyboardDetection(pkt->report, pkt->timestamp_us);
        }
    }
}

static void mouseDetection(uint8_t* mouse_data)
{

}

static void keyboardDetection(uint8_t* keyboard_data, uint32_t time)
{
    /********* Speed Detection **********/

    bool any_key_down = keyboard_data[0] != 0x00;
    for(int i = 2; i < 8; i++)
        any_key_down |= (keyboard_data[i] != 0x00);

    bool is_new_event = memcmp(keyboard_data, kb_prev_report, 8) != 0;

    if(any_key_down && is_new_event)
    {
        if(kb_last != 0)
        {
            uint32_t interval = time - kb_last;
            if(interval < BOT_KB_MIN_INTERVAL_US)
            {
                while(1); //gg
            }
        }
        kb_last = time;
    }

    memcpy(kb_prev_report, keyboard_data, 8);

    /********* Regular Timing Detection **********/
}