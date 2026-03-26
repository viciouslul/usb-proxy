#pragma once

#include <stdbool.h>
#include "proxy_data.h"

void ui_init(void);
void ui_task(void);

void ui_on_scroll_button(void);
void ui_on_select_button(void);

void ui_on_enumeration_result(bool ok);
void ui_on_security_error(void);
void ui_on_unmount(void);

proxy_device_t ui_get_selected_device(void);
bool ui_has_selected_device(void);