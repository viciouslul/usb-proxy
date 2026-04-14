#pragma once

#include "tusb.h"
#include <stdint.h>

#define KEY_COUNT       9
#define KEY_DEBOUNCE_MS 50

static const uint8_t key_pins[KEY_COUNT] = {2, 3, 4, 5, 6, 7, 8, 9, 10};

typedef struct 
{
    uint8_t modifier; 
    uint8_t keycode;  
} key_binding_t;

static const key_binding_t keymap[KEY_COUNT] = 
{
    { 0, HID_KEY_F13 }, /* Key 0 */
    { 0, HID_KEY_F14 }, /* Key 1 */
    { 0, HID_KEY_F15 }, /* Key 2 */
    { 0, HID_KEY_F16 }, /* Key 3 */
    { 0, HID_KEY_F17 }, /* Key 4 */
    { 0, HID_KEY_F18 }, /* Key 5 */
    { 0, HID_KEY_F19 }, /* Key 6 */
    { 0, HID_KEY_F20 }, /* Key 7 */
    { 0, HID_KEY_F21 }, /* Key 8 */
};
