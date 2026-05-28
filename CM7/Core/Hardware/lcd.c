#include "stdlib.h"
#include "lcd.h"
#include "lcdfont.h"
#include "delay.h"
#include "ltdc.h"
#include "stdio.h"

uint32_t g_point_color = RED;
uint32_t g_back_color  = 0XFFFFFFFF;

_lcd_dev lcddev;

void lcd_write_ram_prepare(void) {}

uint32_t lcd_rgb565torgb888(uint16_t rgb565)
{
    uint16_t r, g, b;
    uint32_t rgb888;
    r = (rgb565 & 0XF800) >> 8;
    g = (rgb565 & 0X07E0) >> 3;
    b = (rgb565 & 0X001F) << 3;
    rgb888 = (r << 16) | (g << 8) | b;
    return rgb888;
}

uint32_t lcd_read_point(uint16_t x, uint16_t y)
{
    if (x >= lcddev.width || y >= lcddev.height) return 0;
    return ltdc_read_point(x, y);
}

void lcd_display_on(void)
{
    if (lcdltdc.pwidth != 0)
        ltdc_switch(1);
}

void lcd_display_off(void)
{
    if (lcdltdc.pwidth != 0)
        ltdc_switch(0);
}

void lcd_set_cursor(uint16_t x, uint16_t y) {}
void lcd_scan_dir(uint8_t dir) {}

void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
    if (lcdltdc.pwidth != 0)
        ltdc_draw_point(x, y, color);
}

void lcd_display_dir(uint8_t dir)
{
    lcddev.dir = dir;
    if (lcdltdc.pwidth != 0)
    {
        ltdc_display_dir(dir);
        lcddev.width = lcdltdc.width;
        lcddev.height = lcdltdc.height;
        return;
    }
}

void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    if (lcdltdc.pwidth != 0)
        return;
}

void lcd_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    LCD_PWREN_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = LCD_PWREN_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LCD_PWREN_GPIO_PORT, &gpio_init_struct);

    LCD_PWREN(1);
    delay_ms(100);

    lcddev.id = ltdc_panelid_read();

#if RGB_80_8001280
    lcddev.id = 0X8081;
#endif

    if (lcddev.id != 0)
        ltdc_init();

    printf("LCD ID:%x\r\n", lcddev.id);

    if (lcdltdc.pwidth != 0)
        lcd_display_dir(1);

    LCD_BL(1);
    lcd_clear(WHITE);
}

void lcd_clear(uint32_t color)
{
    if (lcdltdc.pwidth != 0)
        ltdc_clear(color);
}

void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    if (lcdltdc.pwidth != 0)
        ltdc_fill(sx, sy, ex, ey, color);
}

void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    if (lcdltdc.pwidth != 0)
        ltdc_color_fill(sx, sy, ex, ey, color);
}

void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;
    else { incx = -1; delta_x = -delta_x; }

    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;
    else { incy = -1; delta_y = -delta_y; }

    if ( delta_x > delta_y) distance = delta_x;
    else distance = delta_y;

    for (t = 0; t <= distance + 1; t++ )
    {
        lcd_draw_point(row, col, color);
        xerr += delta_x ;
        yerr += delta_y ;

        if (xerr > distance) { xerr -= distance; row += incx; }
        if (yerr > distance) { yerr -= distance; col += incy; }
    }
}

void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color)
{
    if ((len == 0) || (x > lcddev.width) || (y > lcddev.height)) return;
    lcd_fill(x, y, x + len - 1, y, color);
}

void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
    lcd_draw_line(x1, y1, x2, y1, color);
    lcd_draw_line(x1, y1, x1, y2, color);
    lcd_draw_line(x1, y2, x2, y2, color);
    lcd_draw_line(x2, y1, x2, y2, color);
}

void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint32_t color)
{
    int a, b;
    int di;
    a = 0; b = r;
    di = 3 - (r << 1);

    while (a <= b)
    {
        lcd_draw_point(x0 + a, y0 - b, color);
        lcd_draw_point(x0 + b, y0 - a, color);
        lcd_draw_point(x0 + b, y0 + a, color);
        lcd_draw_point(x0 + a, y0 + b, color);
        lcd_draw_point(x0 - a, y0 + b, color);
        lcd_draw_point(x0 - b, y0 + a, color);
        lcd_draw_point(x0 - a, y0 - b, color);
        lcd_draw_point(x0 - b, y0 - a, color);
        a++;

        if (di < 0)
            di += 4 * a + 6;
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }
    }
}

void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint32_t color)
{
    uint32_t i;
    uint32_t imax = ((uint32_t)r * 707) / 1000 + 1;
    uint32_t sqmax = (uint32_t)r * (uint32_t)r + (uint32_t)r / 2;
    uint32_t xr = r;

    lcd_draw_hline(x - r, y, 2 * r, color);

    for (i = 1; i <= imax; i++)
    {
        if ((i * i + xr * xr) > sqmax)
        {
            if (xr > imax)
            {
                lcd_draw_hline (x - i + 1, y + xr, 2 * (i - 1), color);
                lcd_draw_hline (x - i + 1, y - xr, 2 * (i - 1), color);
            }
            xr--;
        }
        lcd_draw_hline(x - xr, y + i, 2 * xr, color);
        lcd_draw_hline(x - xr, y - i, 2 * xr, color);
    }
}

void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint32_t color)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint8_t csize = 0;
    uint8_t *pfont = 0;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);
    chr = chr - ' ';

    switch (size)
    {
        case 12: pfont = (uint8_t *)asc2_1206[chr];  break;
        case 16: pfont = (uint8_t *)asc2_1608[chr];  break;
        case 24: pfont = (uint8_t *)asc2_2412[chr];  break;
        case 32: pfont = (uint8_t *)asc2_3216[chr];  break;
        default: return;
    }

    for (t = 0; t < csize; t++)
    {
        temp = pfont[t];

        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
                lcd_draw_point(x, y, color);
            else if (mode == 0)
                lcd_draw_point(x, y, g_back_color);

            temp <<= 1;
            y++;

            if (y >= lcddev.height) return;

            if ((y - y0) == size)
            {
                y = y0;
                x++;
                if (x >= lcddev.width) return;
                break;
            }
        }
    }
}

static uint32_t lcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}

void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint32_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                lcd_show_char(x + (size / 2) * t, y, ' ', size, 0, color);
                continue;
            }
            else
                enshow = 1;
        }
        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, 0, color);
    }
}

void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint32_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                if (mode & 0X80)
                    lcd_show_char(x + (size / 2) * t, y, '0', size, mode & 0X01, color);
                else
                    lcd_show_char(x + (size / 2) * t, y, ' ', size, mode & 0X01, color);
                continue;
            }
            else
                enshow = 1;
        }
        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, mode & 0X01, color);
    }
}

void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint32_t color)
{
    uint8_t x0 = x;
    width += x;
    height += y;

    while ((*p <= '~') && (*p >= ' '))
    {
        if (x >= width) { x = x0; y += size; }
        if (y >= height) break;

        lcd_show_char(x, y, *p, size, 0, color);
        x += size / 2;
        p++;
    }
}
