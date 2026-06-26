/**
 * LCD 应用层 — 绘图函数、颜色定义、字符串/数字显示
 * =====================================================
 * 初始化:    lcd_init()                      上电→识别面板→初始化 LTDC
 * 清屏:      lcd_clear(WHITE / BLACK / RED / ...)
 * 区域填充:  lcd_fill(sx, sy, ex, ey, color)
 * 画点:      lcd_draw_point(x, y, color)
 * 画线:      lcd_draw_line(x1, y1, x2, y2, color)
 * 画矩形:    lcd_draw_rectangle(x1, y1, x2, y2, color)
 * 画圆:      lcd_draw_circle(x0, y0, r, color)
 * 填充圆:    lcd_fill_circle(x, y, r, color)
 * 显示字符:  lcd_show_char(x, y, 'A', size, mode, color)    size: 12/16/24/32
 * 显示字符串: lcd_show_string(x, y, width, height, size, str, color)
 * 显示数字:  lcd_show_num(x, y, num, len, size, color)
 * 背光:      LCD_BL(1/0)
 * 面板信息:  全局变量 lcddev.width / lcddev.height / lcddev.id
 */
#ifndef __LCD_H
#define __LCD_H

#include "stdlib.h"
#include "sys_util.h"
#include "ltdc_draw.h"

/* LCD_PWREN: PI11 controls LCD_5V power */
#define LCD_PWREN_GPIO_PORT               GPIOI
#define LCD_PWREN_GPIO_PIN                GPIO_PIN_11
#define LCD_PWREN_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)

#define LCD_PWREN(x)      do{ x ? \
                              HAL_GPIO_WritePin(LCD_PWREN_GPIO_PORT, LCD_PWREN_GPIO_PIN, GPIO_PIN_SET) : \
                              HAL_GPIO_WritePin(LCD_PWREN_GPIO_PORT, LCD_PWREN_GPIO_PIN, GPIO_PIN_RESET); \
                          }while(0)

/* LCD parameters */
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t id;
    uint8_t dir;
} _lcd_dev;

extern _lcd_dev lcddev;
extern uint32_t g_point_color;
extern uint32_t g_back_color;

#define LCD_BL(x)      do{ x ? \
                           HAL_GPIO_WritePin(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, GPIO_PIN_SET) : \
                           HAL_GPIO_WritePin(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, GPIO_PIN_RESET); \
                       }while(0)

#define LCD_RST(x)     do{ x ? \
                           HAL_GPIO_WritePin(LTDC_RST_GPIO_PORT, LTDC_RST_GPIO_PIN, GPIO_PIN_SET) : \
                           HAL_GPIO_WritePin(LTDC_RST_GPIO_PORT, LTDC_RST_GPIO_PIN, GPIO_PIN_RESET); \
                       }while(0)

/* Colors (RGB565) */
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB565

#define WHITE           0xFFFF
#define BLACK           0x0000
#define RED             0xF800
#define GREEN           0x07E0
#define BLUE            0x001F
#define MAGENTA         0XF81F
#define YELLOW          0XFFE0
#define CYAN            0X07FF
#define BROWN           0XBC40
#define BRRED           0XFC07
#define GRAY            0X8430
#define DARKBLUE        0X01CF
#define LIGHTBLUE       0X7D7C
#define GRAYBLUE        0X5458
#define LIGHTGREEN      0X841F
#define LGRAY           0XC618
#define LGRAYBLUE       0XA651
#define LBBLUE          0X2B12

#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888

#define WHITE           0xFFFFFF
#define BLACK           0x000000
#define RED             0xFF0000
#define GREEN           0x00FF00
#define BLUE            0x0000FF
#define MAGENTA         0XFF00FF
#define YELLOW          0XFFFF00
#define CYAN            0X00FFFF
#define BROWN           0xB88800
#define BRRED           0XF88038
#define GRAY            0X808480
#define DARKBLUE        0X003878
#define LIGHTBLUE       0X78ACE0
#define GRAYBLUE        0X5088C0
#define LIGHTGREEN      0X8080F8
#define LGRAY           0XC0C0C0
#define LGRAYBLUE       0XA0C888
#define LBBLUE          0x286090

#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888

#define WHITE           0xFFFFFFFF
#define BLACK           0xFF000000
#define RED             0xFFFF0000
#define GREEN           0xFF00FF00
#define BLUE            0xFF0000FF
#define MAGENTA         0XFFFF00FF
#define YELLOW          0XFFFFFF00
#define CYAN            0XFF00FFFF
#define BROWN           0xFFB88800
#define BRRED           0xFFF88038
#define GRAY            0xFF808480
#define DARKBLUE        0xFF003878
#define LIGHTBLUE       0xFF78ACE0
#define GRAYBLUE        0xFF5088C0
#define LIGHTGREEN      0xFF8080F8
#define LGRAY           0xFFC0C0C0
#define LGRAYBLUE       0xFFA0C888
#define LBBLUE          0xFF286090

#endif

void lcd_init(void);
void lcd_display_on(void);
void lcd_display_off(void);
void lcd_scan_dir(uint8_t dir);
void lcd_display_dir(uint8_t dir);
void lcd_write_ram_prepare(void);
void lcd_set_cursor(uint16_t x, uint16_t y);
uint32_t lcd_rgb565torgb888(uint16_t rgb565);
uint32_t lcd_read_point(uint16_t x, uint16_t y);
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color);
void lcd_clear(uint32_t color);
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint32_t color);
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint32_t color);
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color);
void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint32_t color);
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint32_t color);
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint32_t color);
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint32_t color);

#endif
