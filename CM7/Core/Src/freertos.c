#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"

#include "sys_util.h"
#include "delay.h"
#include "lcd.h"
#include <stdio.h>

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void) {
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
}

/* Color palette for test sequences */
static const uint32_t g_colors[] = {
    RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, BROWN, BRRED,
    DARKBLUE, LIGHTBLUE, GRAYBLUE, LIGHTGREEN, LBBLUE
};
#define COLOR_COUNT (sizeof(g_colors) / sizeof(g_colors[0]))

void StartDefaultTask(void *argument)
{
    uint8_t  step = 0;
    uint8_t  ci = 0;   /* color index */
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;
    char     buf[32];

    for (;;)
    {
        switch (step)
        {
        /* ---- solid color fills ---- */
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            lcd_clear(g_colors[ci]);
            ci = (ci + 1) % COLOR_COUNT;
            osDelay(800);
            break;

        /* ---- gradient bars ---- */
        case 10:
            for (uint16_t y = 0; y < h; y++)
            {
                uint32_t color = (y < h / 2)
                    ? RED
                    : BLUE;
                lcd_draw_line(0, y, w, y, color);
            }
            osDelay(1200);
            break;

        /* ---- center cross & circles ---- */
        case 11:
            lcd_clear(BLACK);
            lcd_draw_line(w / 2, 0, w / 2, h, WHITE);
            lcd_draw_line(0, h / 2, w, h / 2, WHITE);
            lcd_draw_circle(w / 2, h / 2, 30, GREEN);
            lcd_draw_circle(w / 2, h / 2, 60, RED);
            lcd_draw_circle(w / 2, h / 2, 90, BLUE);
            osDelay(1500);
            break;

        /* ---- four-corner rectangles ---- */
        case 12:
            lcd_clear(BLACK);
            lcd_draw_rectangle(10, 10, 150, 100, RED);
            lcd_draw_rectangle(w - 160, 10, w - 10, 100, GREEN);
            lcd_draw_rectangle(10, h - 110, 150, h - 10, BLUE);
            lcd_draw_rectangle(w - 160, h - 110, w - 10, h - 10, YELLOW);
            lcd_show_string(25, 40, 120, 32, 24, "RED", RED);
            lcd_show_string(w - 145, 40, 120, 32, 24, "GREEN", GREEN);
            lcd_show_string(25, h - 75, 120, 32, 24, "BLUE", BLUE);
            lcd_show_string(w - 145, h - 75, 120, 32, 24, "YELLOW", YELLOW);
            osDelay(2000);
            break;

        /* ---- info screen ---- */
        case 13:
            lcd_clear(DARKBLUE);
            lcd_show_string(10, 20, w - 20, 32, 32, "STM32H747 LCD Test", WHITE);
            lcd_show_string(10, 65, w - 20, 24, 24, "LTDC 800x480 RGB565", LIGHTBLUE);
            lcd_show_string(10, 100, w - 20, 16, 16, "FreeRTOS CMSIS_V2", LGRAY);

            sprintf(buf, "LCD ID: %04X", lcddev.id);
            lcd_show_string(10, 140, w - 20, 16, 16, buf, YELLOW);

            sprintf(buf, "Resolution: %d x %d", w, h);
            lcd_show_string(10, 170, w - 20, 16, 16, buf, CYAN);

            lcd_fill_circle(w / 2, 300, 40, RED);
            lcd_fill_circle(w / 2 - 80, 320, 30, GREEN);
            lcd_fill_circle(w / 2 + 80, 320, 30, BLUE);

            lcd_show_string(10, 400, w - 20, 16, 16, "Bare-metal LCD + CubeMX HAL", LGRAY);
            osDelay(3000);
            break;

        default:
            step = 0;
            break;
        }

        step++;
        if (step > 13) step = 0;
    }
}
