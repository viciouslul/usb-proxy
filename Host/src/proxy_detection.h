#pragma once
#include <stdint.h>
#include "proxy_data.h"
#include "tusb.h"

#define STRIKE_TIMEOUT_US 5000000 // 5 seconds in microseconds
#define BOT_AVG_THRESHOLD 30000 //30 ms
#define MIN_SPREAD_THRESHOLD 5000 //5ms
#define BOT_MOUSE_MIN_INTERVAL_MS 20
#define BOT_KB_MIN_INTERVAL_MS 50
#define hid_keyboard_report_t 8


void botDetection(proxy_packet_t* pkt);
void botDetection_reset(void);


