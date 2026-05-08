#include "proxy_ssd1306.h"
#include <string.h>

lcd_t lcd = {0};

static void lcd_cmd(lcd_t *lcd, uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    i2c_write_blocking(lcd->i2c, lcd->addr, data, 2, false);
}

static void lcd_data(lcd_t *lcd, const uint8_t *data, size_t len)
{
    uint8_t temp[17];
    temp[0] = 0x40;

    while (len)
    {
        size_t chunk = len > 16 ? 16 : len;
        memcpy(&temp[1], data, chunk);
        i2c_write_blocking(lcd->i2c, lcd->addr, temp, chunk + 1, false);
        data += chunk;
        len -= chunk;
    }
}

void lcd_init(lcd_t *lcd, i2c_inst_t *i2c, uint8_t addr)
{
    lcd->i2c  = i2c;
    lcd->addr = addr;
    sleep_ms(100);

    lcd_cmd(lcd, 0xAE);
    lcd_cmd(lcd, 0xD5);
    lcd_cmd(lcd, 0x80);
    lcd_cmd(lcd, 0xA8);
    lcd_cmd(lcd, 0x1F);
    lcd_cmd(lcd, 0xD3);
    lcd_cmd(lcd, 0x00);
    lcd_cmd(lcd, 0x40);
    lcd_cmd(lcd, 0x8D);
    lcd_cmd(lcd, 0x14);
    lcd_cmd(lcd, 0x20);
    lcd_cmd(lcd, 0x00);
    lcd_cmd(lcd, 0xA1);
    lcd_cmd(lcd, 0xC8);
    lcd_cmd(lcd, 0xDA);
    lcd_cmd(lcd, 0x00);
    lcd_cmd(lcd, 0x81);
    lcd_cmd(lcd, 0x8F);
    lcd_cmd(lcd, 0xD9);
    lcd_cmd(lcd, 0x1F);
    lcd_cmd(lcd, 0xDB);
    lcd_cmd(lcd, 0x40);
    lcd_cmd(lcd, 0xA4);
    lcd_cmd(lcd, 0xA6);

    lcd_clear(lcd);
    lcd_show(lcd);

    lcd_cmd(lcd, 0xAF);
    sleep_ms(100);
}

void lcd_clear(lcd_t *lcd)
{
    memset(lcd->buf, 0x00, sizeof(lcd->buf));
}

void lcd_show(lcd_t *lcd)
{
    lcd_cmd(lcd, 0x21);
    lcd_cmd(lcd, 0x00);
    lcd_cmd(lcd, LCD_WIDTH - 1);

    lcd_cmd(lcd, 0x22);
    lcd_cmd(lcd, 0x00);
    lcd_cmd(lcd, (LCD_HEIGHT / 8) - 1);

    lcd_data(lcd, lcd->buf, sizeof(lcd->buf));
}

void lcd_write_line(lcd_t *lcd, int row, const char *text)
{
    if (row < 0 || row >= LCD_ROWS)
        return;

    int top_base = row * 2 * LCD_WIDTH;
    int bot_base = top_base + LCD_WIDTH;

    memset(&lcd->buf[top_base], 0x00, LCD_WIDTH);
    memset(&lcd->buf[bot_base], 0x00, LCD_WIDTH);

    int x = 0;

    while (*text && x + FONT_W <= LCD_WIDTH)
    {
        unsigned char c = (unsigned char)*text++;

        if (c >= sizeof(font_8x16) / sizeof(font_8x16[0]))
            c = ' ';

        for (int i = 0; i < FONT_W; i++)
        {
            lcd->buf[top_base + x + i] = font_8x16[c][i];
            lcd->buf[bot_base + x + i] = font_8x16[c][i + 8];
        }

        x += FONT_W;
    }
}
