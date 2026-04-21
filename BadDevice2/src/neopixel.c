#include "neopixel.h"

/* pioasm 2.2.0 emits ws2812_T1/T2/T3 defines but references bare T1/T2/T3 in
   the generated ws2812_program_init — alias them before the header is pulled in. */
#define T1 ws2812_T1
#define T2 ws2812_T2
#define T3 ws2812_T3
#include "ws2812.pio.h"
#undef T1
#undef T2
#undef T3
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

#define WS2812_FREQ 800000.0f

static PIO     np_pio = pio0;
static uint    np_sm  = 0;

/* GRB pixel buffer — WS2812 expects green first, then red, then blue */
static uint32_t pixel_buf[NEOPIXEL_COUNT];

static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
}

void neopixel_init(int pin)
{
    uint offset = pio_add_program(np_pio, &ws2812_program);
    ws2812_program_init(np_pio, np_sm, offset, pin, WS2812_FREQ, false);

    for (int i = 0; i < NEOPIXEL_COUNT; i++)
        pixel_buf[i] = 0;

    neopixel_show();
}

void neopixel_set(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= NEOPIXEL_COUNT)
        return;
    pixel_buf[index] = rgb_to_grb(r, g, b);
}

void neopixel_fill(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t grb = rgb_to_grb(r, g, b);
    for (int i = 0; i < NEOPIXEL_COUNT; i++)
        pixel_buf[i] = grb;
}

void neopixel_show(void)
{
    /* PIO shifts MSB-first: data must be in the upper 24 bits of the word */
    for (int i = 0; i < NEOPIXEL_COUNT; i++)
        pio_sm_put_blocking(np_pio, np_sm, pixel_buf[i] << 8u);

    sleep_us(60); /* >50 µs reset pulse to latch the frame */
}
