#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"

#include "sys_util.h"
#include "delay.h"
#include "lcd.h"
#include "ltdc_draw.h"
#include "app_touchgfx.h"
#include <stdio.h>

/* VSYNC diagnostic counter — incremented by LTDC line ISR */
extern volatile uint32_t g_vsync_count;

/* DMA2D ISR counter — incremented every time DMA2D ISR fires */
extern volatile uint32_t g_dma2d_isr_count;

/* TouchGFX rendering pipeline counters */
extern volatile uint32_t g_beginframe_count;
extern volatile uint32_t g_flush_count;
extern volatile uint32_t g_rendered_pixels;
extern volatile uint32_t g_task_entry_called;
extern volatile uint32_t g_hal_init_enter;
extern volatile uint32_t g_hal_init_done;

/* Software VSYNC trigger — unblocks TouchGFX even if LTDC interrupt is broken */
extern void software_vsync_trigger(void);

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t touchgfxTaskHandle;
const osThreadAttr_t touchgfxTask_attributes = {
  .name = "TouchGFX",
  .stack_size = 12288 * 4,  /* 48 KB — software rendering needs deep call stacks */
  .priority = (osPriority_t) osPriorityAboveNormal,
};

void StartDefaultTask(void *argument);
void StartTouchGFXTask(void *argument);

void MX_FREERTOS_Init(void) {
  defaultTaskHandle  = osThreadNew(StartDefaultTask,  NULL, &defaultTask_attributes);
  touchgfxTaskHandle = osThreadNew(StartTouchGFXTask, NULL, &touchgfxTask_attributes);
}

void StartTouchGFXTask(void *argument)
{
    volatile uint32_t *marker = (volatile uint32_t *)0xD0000300U;

    marker[0] = 0xAAAA0001;  /* TouchGFX task started */

    MX_TouchGFX_Init();

    marker[1] = 0xAAAA0002;  /* MX_TouchGFX_Init() completed */
    /* Ensure backlight stays ON — TouchGFX will handle the display */
    HAL_GPIO_WritePin(BL_CTR_GPIO_Port, BL_CTR_Pin, GPIO_PIN_SET);

    marker[2] = 0xAAAA0003;  /* entering event loop */
    MX_TouchGFX_Process();  /* enters event loop, never returns */
}

/* Color palette for test sequences */
static const uint32_t g_colors[] = {
    RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, BROWN, BRRED,
    DARKBLUE, LIGHTBLUE, GRAYBLUE, LIGHTGREEN, LBBLUE
};
#define COLOR_COUNT (sizeof(g_colors) / sizeof(g_colors[0]))

void StartDefaultTask(void *argument)
{
    /* TouchGFX now handles the framebuffer — don't draw stripes,
       they would overwrite TouchGFX's rendering at the same address. */
    for (;;)
    {
        osDelay(1000);
    }
}
