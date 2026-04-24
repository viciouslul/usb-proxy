#pragma once

#include "proxy_data.h"
#include <stdbool.h>

void ui_init(void);
void ui_task(void);

void ui_on_scroll_button(void);
void ui_on_select_button(void);

void ui_set_vid_pid(uint16_t vid, uint16_t pid);
void ui_on_enumeration_result(bool ok);
void ui_on_security_error(void);
void ui_on_unmount(void);
void ui_on_reenumeration(proxy_device_t old_dev, proxy_device_t new_dev);
void ui_on_idle_timeout(void);

proxy_device_t ui_get_selected_device(void);
bool           ui_has_selected_device(void);
bool           ui_is_whitelist_enabled(void);
bool           ui_is_whitelist_add_mode(void);
void           ui_on_whitelist_device_connected(uint16_t vid, uint16_t pid);