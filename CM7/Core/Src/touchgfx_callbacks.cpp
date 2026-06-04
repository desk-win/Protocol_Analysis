/**
  ******************************************************************************
  * File Name          : touchgfx_callbacks.cpp
  * Description        : LTDC callback bridge — connects HAL ISR to TouchGFX OSWrappers
  ******************************************************************************
  */

/* STM32 HAL must come before TouchGFX headers to define HAL_StatusTypeDef etc. */
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_ltdc.h"

#include <touchgfx/hal/OSWrappers.hpp>

/* ── VSYNC diagnostics: incremented every time the LTDC line interrupt fires.
   Visible to freertos.c for debug checking. ── */
extern "C" {
volatile uint32_t g_vsync_count = 0;
}

extern "C" {

/**
 * @brief  LTDC Line Event callback (called from HAL_LTDC_IRQHandler on LIE).
 *         Signals TouchGFX that a VSYNC-like event has occurred.
 * @param  hltdc: LTDC handle (unused)
 */
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    (void)hltdc;
    g_vsync_count++;
    touchgfx::OSWrappers::signalVSync();
}

/**
 * @brief  Software VSYNC trigger — call this from a timer or task
 *         to unblock TouchGFX when the LTDC line interrupt is not working.
 */
void software_vsync_trigger(void)
{
    g_vsync_count++;
    touchgfx::OSWrappers::signalVSync();
}

} // extern "C"
