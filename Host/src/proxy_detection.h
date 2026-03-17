#pragma once

#include <stdint.h>
#include "proxy_data.h"
#include "tusb.h"


#define BOT_MOUSE_MIN_INTERVAL_MS 20
#define BOT_KB_MIN_INTERVAL_MS 30
#define BOT_KB_MIN_INTERVAL_US (BOT_KB_MIN_INTERVAL_MS * 1000)
#define hid_keyboard_report_t 8

void botDetection(proxy_packet_t* pkt);


