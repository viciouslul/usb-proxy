#pragma once

#include <stdint.h>

/* Must match KEY_COUNT in keymap.h */
#define NEOPIXEL_COUNT 9

void neopixel_init(int pin);

/* Set one pixel in the buffer (0-indexed) */
void neopixel_set(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/* Set all pixels to the same color */
void neopixel_fill(uint8_t r, uint8_t g, uint8_t b);

/* Push buffer to the LED chain (~330 µs, includes reset pulse) */
void neopixel_show(void);
