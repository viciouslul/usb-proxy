#include "proxy_data.h"

proxy_queue_t proxy_queue = {0};
volatile screen_page_t current_screen = SCREEN_KEYBOARD;
volatile bool update_display = true;
uint8_t volatile device_selected = NONE;