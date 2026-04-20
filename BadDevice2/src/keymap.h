#pragma once

#include "tusb.h"
#include <stdint.h>

#define KEY_COUNT 6
#define KEY_DEBOUNCE_MS 50

static const uint8_t key_pins[KEY_COUNT] = {6, 7, 8, 9, 10, 11};

typedef struct
{
  uint8_t modifier;
  uint8_t keycode;
} key_binding_t;

static const key_binding_t keymap[KEY_COUNT] = {
    {0, HID_KEY_0},
    {0, HID_KEY_1},
    {0, HID_KEY_2},
    {0, HID_KEY_3},
    {0, HID_KEY_4},
    {0, HID_KEY_5},
};