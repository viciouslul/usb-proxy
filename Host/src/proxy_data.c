#include "proxy_data.h"

proxy_queue_t proxy_queue = {0};
volatile screen_page_t current_screen = SCREEN_WELCOME;
volatile bool update_display = false;
uint8_t volatile device_selected = 0;