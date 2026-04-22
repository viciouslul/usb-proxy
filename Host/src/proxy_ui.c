#include "proxy_ui.h"

#include "proxy_ssd1306.h"
#include "pico/stdlib.h"
#include <stdio.h>

typedef enum
{
    UI_SCREEN_SELECT_KEYBOARD = 0,
    UI_SCREEN_SELECT_MOUSE,
    UI_SCREEN_SELECT_MSC,
    UI_SCREEN_DEVICE_SELECTED,
    UI_SCREEN_DEVICE_UNMOUNTED,
    UI_SCREEN_ENUM_OK,
    UI_SCREEN_ENUM_ERROR,
    UI_SCREEN_BOT_DETECTED,
    UI_SCREEN_REENUM_DETECTED,
    UI_SCREEN_IDLE_TIMEOUT,
} ui_screen_t;

static ui_screen_t current_screen = UI_SCREEN_SELECT_KEYBOARD;
static bool update_display = true;
static proxy_device_t selected_device = HID_NONE;
static uint16_t current_vid = 0;
static uint16_t current_pid = 0;
static proxy_device_t reenum_old_dev = HID_NONE;
static proxy_device_t reenum_new_dev = HID_NONE;

static const proxy_device_t menu_devices[] = {
    HID_KEYBOARD,
    HID_MOUSE,
    MSC,
};

static const char* selected_device_label(proxy_device_t dev)
{
    switch (dev)
    {
        case HID_KEYBOARD: return "Keyboard";
        case HID_MOUSE:    return "Mouse";
        case MSC:          return "MSC";
        default:           return "None";
    }
}

static const char* device_short_label(proxy_device_t dev)
{
    switch (dev)
    {
        case HID_KEYBOARD: return "KBD";
        case HID_MOUSE:    return "MSE";
        case MSC:          return "MSC";
        default:           return "?";
    }
}

static inline ui_screen_t menu_screen_from_index(uint8_t index)
{
    return (ui_screen_t)(UI_SCREEN_SELECT_KEYBOARD + index);
}

static inline uint8_t menu_index_from_screen(ui_screen_t screen)
{
    if (screen > UI_SCREEN_SELECT_MSC)
    {
        return 0;
    }
    return (uint8_t)(screen - UI_SCREEN_SELECT_KEYBOARD);
}

void ui_init(void)
{
    current_screen = UI_SCREEN_SELECT_KEYBOARD;
    update_display = true;
    selected_device = HID_NONE;
    current_vid = 0;
    current_pid = 0;
}

void ui_task(void)
{
    if (!update_display) return;

    switch (current_screen)
    {
        case UI_SCREEN_SELECT_KEYBOARD:
            lcd_write_line(&lcd, 0, "Select Device:");
            lcd_write_line(&lcd, 1, "Keyboard");
            lcd_show(&lcd);
            update_display = false;
            break;

        case UI_SCREEN_SELECT_MOUSE:
            lcd_write_line(&lcd, 0, "Select Device:");
            lcd_write_line(&lcd, 1, "Mouse");
            lcd_show(&lcd);
            update_display = false;
            break;

        case UI_SCREEN_SELECT_MSC:
            lcd_write_line(&lcd, 0, "Select Device:");
            lcd_write_line(&lcd, 1, "MSC");
            lcd_show(&lcd);
            update_display = false;
            break;

        case UI_SCREEN_DEVICE_SELECTED:
            lcd_write_line(&lcd, 0, "Device selected");
            lcd_write_line(&lcd, 1, selected_device_label(selected_device));
            lcd_show(&lcd);
            update_display = false;
            break;

        case UI_SCREEN_DEVICE_UNMOUNTED:
            lcd_write_line(&lcd, 0, "Device");
            lcd_write_line(&lcd, 1, "unmounted");
            lcd_show(&lcd);
            sleep_ms(2500);
            current_screen = UI_SCREEN_SELECT_KEYBOARD;
            update_display = true;
            break;

        case UI_SCREEN_ENUM_OK:
        {
            char vid_pid_line[17] = {0};
            snprintf(vid_pid_line, sizeof(vid_pid_line), "%04X:%04X", current_vid, current_pid);
            lcd_write_line(&lcd, 0, "Ready to use");
            lcd_write_line(&lcd, 1, vid_pid_line);
            lcd_show(&lcd);
            update_display = false;
            break;
        }

        case UI_SCREEN_ENUM_ERROR:
            selected_device = HID_NONE;
            lcd_write_line(&lcd, 0, "ERROR");
            lcd_write_line(&lcd, 1, "Enumeration");
            lcd_show(&lcd);
            sleep_ms(2500);
            current_screen = UI_SCREEN_SELECT_KEYBOARD;
            update_display = true;
            break;

        case UI_SCREEN_BOT_DETECTED:
            selected_device = HID_NONE;
            lcd_write_line(&lcd, 0, "Security Alert");
            lcd_write_line(&lcd, 1, "Bot detected!");
            lcd_show(&lcd);
            sleep_ms(5000);
            current_screen = UI_SCREEN_SELECT_KEYBOARD;
            update_display = true;
            break;

        case UI_SCREEN_REENUM_DETECTED:
        {
            selected_device = HID_NONE;
            char transition[17] = {0};
            snprintf(transition, sizeof(transition), "%s -> %s",
                     device_short_label(reenum_old_dev),
                     device_short_label(reenum_new_dev));
            lcd_write_line(&lcd, 0, "RE-ENUM Detected");
            lcd_write_line(&lcd, 1, transition);
            lcd_show(&lcd);
            sleep_ms(5000);
            current_screen = UI_SCREEN_SELECT_KEYBOARD;
            update_display = true;
            break;
        }

        case UI_SCREEN_IDLE_TIMEOUT:
            selected_device = HID_NONE;
            lcd_write_line(&lcd, 0, "Idle timeout");
            lcd_write_line(&lcd, 1, "AFK protection");
            lcd_show(&lcd);
            sleep_ms(3000);
            current_screen = UI_SCREEN_SELECT_KEYBOARD;
            update_display = true;
            break;

        default:
            break;
    }
}

void ui_on_scroll_button(void)
{
    if (selected_device != HID_NONE) return;

    uint8_t index = menu_index_from_screen(current_screen);
    index = (uint8_t)((index + 1) % (sizeof(menu_devices) / sizeof(menu_devices[0])));
    current_screen = menu_screen_from_index(index);
    update_display = true;
}

void ui_on_select_button(void)
{
    uint8_t index = menu_index_from_screen(current_screen);
    selected_device = menu_devices[index];
    current_screen = UI_SCREEN_DEVICE_SELECTED;
    update_display = true;
}

void ui_set_vid_pid(uint16_t vid, uint16_t pid)
{
    current_vid = vid;
    current_pid = pid;
}

void ui_on_enumeration_result(bool ok)
{
    current_screen = ok ? UI_SCREEN_ENUM_OK : UI_SCREEN_ENUM_ERROR;
    update_display = true;
}

void ui_on_security_error(void)
{
    current_screen = UI_SCREEN_BOT_DETECTED;
    update_display = true;
}

void ui_on_unmount(void)
{
    selected_device = HID_NONE;
    current_vid = 0;
    current_pid = 0;
    current_screen = UI_SCREEN_DEVICE_UNMOUNTED;
    update_display = true;
}

void ui_on_reenumeration(proxy_device_t old_dev, proxy_device_t new_dev)
{
    current_vid = 0;
    current_pid = 0;
    reenum_old_dev = old_dev;
    reenum_new_dev = new_dev;
    current_screen = UI_SCREEN_REENUM_DETECTED;
    update_display = true;
}

void ui_on_idle_timeout(void)
{
    selected_device = HID_NONE;
    current_vid = 0;
    current_pid = 0;
    current_screen = UI_SCREEN_IDLE_TIMEOUT;
    update_display = true;
}

proxy_device_t ui_get_selected_device(void)
{
    return selected_device;
}

bool ui_has_selected_device(void)
{
    return selected_device != HID_NONE;
}