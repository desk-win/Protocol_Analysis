/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : STM32TouchController.cpp
  ******************************************************************************
  * This file was created by TouchGFX Generator 4.26.1. This file is only
  * generated once! Delete this file from your project and re-generate code
  * using STM32CubeMX or change this file manually to update it.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN STM32TouchController */

#include <STM32TouchController.hpp>
#include <touch.h>

/* Global: accessible from screenView for visual debug */
extern "C" { int g_tp_init_result = -1; }  /* -1=not called, 0=ok, 1=failed */

void STM32TouchController::init()
{
    /**
     * Initialize touch controller and driver
     *
     * tp_init() auto-detects GT9xxx or FT5206 via software I2C (PB10/PB11).
     * Must be called after lcd_init() (depends on lcddev.dir).
     * This runs in the TouchGFX task, which starts after main() lcd_init().
     */
    g_tp_init_result = 2;  /* debug: 2=init entered */
    g_tp_init_result = tp_init();
}

bool STM32TouchController::sampleTouch(int32_t& x, int32_t& y)
{
    /**
     * Called by TouchGFX framework every tick (~60 Hz).
     *
     * tp_scan() reads I2C only every ~10 frames (throttle) to save bus time.
     * Between reads, tp_dev.sta retains TP_PRES_DOWN, so we check it directly
     * instead of relying solely on tp_scan()'s return value.  This matches the
     * example (实验24) which always checks tp_dev.sta & TP_PRES_DOWN.
     */
    tp_scan(0);

    if (tp_dev.sta & TP_PRES_DOWN)
    {
        uint16_t tx = tp_dev.x[0];
        uint16_t ty = tp_dev.y[0];

        /* Reject invalid coordinates (no touch / out of range) */
        if (tx == 0xFFFF || ty == 0xFFFF)
        {
            return false;
        }

        x = tx;
        y = ty;
        return true;
    }

    return false;
}

/* USER CODE END STM32TouchController */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
