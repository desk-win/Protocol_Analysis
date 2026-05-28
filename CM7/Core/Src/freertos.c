#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"

#include "sys.h"
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

void StartDefaultTask(void *argument)
{
  uint8_t x = 0;
  char lcd_id[12];

  sprintf(lcd_id, "LCD ID:%04X", lcddev.id);

  for(;;)
  {
    switch (x)
    {
        // case 0:  lcd_draw_point(100, 100, RED);    break;
        // case 1:  lcd_draw_line(100, 100, 200, 200, RED);    break;
        // case 2:  lcd_draw_rectangle(100, 100, 200, 200, RED);    break;
        // case 3:  lcd_draw_circle(150, 150, 50, RED);    break;
        // case 4:  lcd_fill_circle(150, 150, 50, RED);    break;
        // case 5:  lcd_show_char(10, 160,'A',16,0, RED);    break;
        // case 6:  lcd_show_num(10, 180, 12345,5, 16, RED);    break;
        // case 7:  lcd_fill(100, 100, 200, 200, RED);   break;
        // case 8:  lcd_show_string(10, 200, 230,32,32,"Hello, World!", RED);    break;
        // case 9:  lcd_clear(GRAY);     break;
        // case 10: lcd_clear(LGRAY);    break;
        // case 11: lcd_clear(BROWN);    break;
    }

    // lcd_show_string(10, 40, 230, 32, 32, "STM32H747 ^_^", RED);
    // lcd_show_string(10, 80, 230, 24, 24, "LTDC LCD TEST", RED);
    // lcd_show_string(10, 110, 230, 16, 16, "WKS SMART", RED);
    // lcd_show_string(10, 130, 230, 16, 16, lcd_id, RED);

    x++;
    if (x == 12) x = 0;

    osDelay(1000);
  }
}
