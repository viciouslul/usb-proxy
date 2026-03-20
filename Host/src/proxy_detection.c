#include "proxy_detection.h"
#include <stdlib.h>

static uint32_t kb_last =           0;
static uint32_t mouse_last =        0;
static uint8_t  kb_prev_keycode =   0x00;
static uint8_t  kb_prev_report[8] = {0};
static uint32_t kb_history[4] =     {0};
static uint8_t  kb_history_idx =    0;
static uint8_t  strike_count =      0;


static void mouseDetection(uint8_t* mouse_data);
static void keyboardDetection(uint8_t* keyboard_data, uint32_t time);

void botDetection(proxy_packet_t* pkt)
{  
#ifdef PROXY_DEBUG
#endif
    if (pkt->msg_t == PROXY_MSG_REPORT)
    {
        if (pkt->dev_t == HID_KEYBOARD)
        {
            /****Debug things***
            tud_cdc_write_str("We are in botDetection, why?\r\n");
            char buf[64];
            sprintf(buf, "report_len: %u\r\n", pkt->report_len);
            tud_cdc_write(buf, strlen(buf));
            tud_cdc_write_flush();

            sprintf(buf, "report: %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
                pkt->report[0], pkt->report[1], pkt->report[2], pkt->report[3],
                pkt->report[4], pkt->report[5], pkt->report[6], pkt->report[7]);
            tud_cdc_write(buf, strlen(buf));
            tud_cdc_write_flush();
            */

            /*do some checks*/
            // 1. Length Check: Catch malformed/malicious packets
            //if (pkt->report_len != 8) while(1);

            // 2. Reserved Byte Check: Catch non-spec-compliant bot hardware
            //if (pkt->report[1] != 0x00) while(1);

            keyboardDetection(pkt->report, pkt->timestamp_us);
        }
    }
}

static void mouseDetection(uint8_t* mouse_data)
{

}

static void keyboardDetection(uint8_t* keyboard_data, uint32_t time)
{
    //rollover guard
    for (int i = 2; i < 8; i++)
    {
        if (keyboard_data[i] == 0x01) return;
    }

    // 1. Check if the report is actually different from the last one
    // and if any key is actually being held down.
    // check if old key was present, we only wanna detect if new one comes in
    bool is_new_report = memcmp(kb_prev_report, keyboard_data, 8) != 0;
    if (!is_new_report) return;

    bool any_key_down = false;
    bool new_key = false;

    for (int i = 2; i < 8; i++)
    {
        if (keyboard_data[i] != 0) //is any key pressed
        {
            any_key_down = true;
            uint8_t key = keyboard_data[i]; //check the key

            bool already_pressed = false;
            for(int j = 2; j < 8; j++) //check if this key was present in the previous report
            {
                if(kb_prev_report[j] == key)
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
    if(new_key && any_key_down)
    {
        if(kb_last != 0)
        {
            uint32_t current_interval = time - kb_last;

            // Store the delay
            kb_history[kb_history_idx] = current_interval;
            kb_history_idx = (kb_history_idx + 1) % 4;

            // Only analyze if we have at least 4 samples
            if(kb_history[3] != 0)
            {
                //calculate mean
                uint32_t sum = 0;
                for(int i = 0; i < 4; i++) sum += kb_history[i];
                uint32_t avg = sum / 4;

                //calculate spread (absolute deviation)
                uint32_t spread = 0;
                for (int i = 0; i < 4; i++)
                {
                    spread += abs((int32_t)kb_history[i] - (int32_t)avg);
                }

                if (avg < BOT_AVG_THRESHOLD && spread < MIN_SPREAD_THRESHOLD)
                {
                    memset(kb_history, 0, sizeof(kb_history));
                    if (strike_count++ >= 3) while(1);
                }
            }
        }
        kb_last = time;
    }

    //reset strike_count after a few seconds
    if((time - kb_last) >= STRIKE_TIMEOUT_US) strike_count = 0;

    // Always update the previous report for the next comparison
    memcpy(kb_prev_report, keyboard_data, 8);
}

void botDetection_reset(void)
{
    kb_last         = 0;
    mouse_last      = 0;
    kb_prev_keycode = 0x00;
    kb_history_idx  = 0;
    strike_count    = 0;
    memset(kb_prev_report, 0, sizeof(kb_prev_report));
    memset(kb_history,     0, sizeof(kb_history));
}