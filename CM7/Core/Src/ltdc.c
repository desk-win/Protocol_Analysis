/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ltdc.c
  * @brief   This file provides code for the configuration
  *          of the LTDC instances.
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
/* Includes ------------------------------------------------------------------*/
#include "ltdc.h"

/* USER CODE BEGIN 0 */
#include "ltdc_draw.h"
#include "delay.h"
#include "tft_spi.h"
#include "lcd.h"
/* USER CODE END 0 */

LTDC_HandleTypeDef hltdc;

/* LTDC init function */
void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */
  /* CRITICAL: Read panel ID BEFORE HAL_LTDC_Init() configures GPIO as LTDC AF.
   * If we read after MspInit, PJ6/PK2/PK6 are already LTDC outputs and
   * the ID read fails → wrong timing → blue tint / display corruption.
   * This matches the example project's order (ltdc_panelid_read before HAL_LTDC_Init). */
  {
      static uint16_t pre_lcdid = 0;
      pre_lcdid = ltdc_panelid_read();
      lcddev.id = pre_lcdid;

      if (pre_lcdid == 0X5571)
          tft_spi_init();

  #if RGB_80_8001280
      pre_lcdid = 0X8081;
  #endif

      /* Set panel timing parameters BEFORE LTDC init */
      if (pre_lcdid == 0X4342)
      {
          lcdltdc.pwidth = 480; lcdltdc.pheight = 272;
          lcdltdc.hsw = 1; lcdltdc.hbp = 40; lcdltdc.hfp = 5;
          lcdltdc.vsw = 1; lcdltdc.vbp = 8;  lcdltdc.vfp = 8;
      }
      else if (pre_lcdid == 0X7084)
      {
          lcdltdc.pwidth = 800; lcdltdc.pheight = 480;
          lcdltdc.hsw = 1; lcdltdc.hbp = 46;  lcdltdc.hfp = 210;
          lcdltdc.vsw = 1; lcdltdc.vbp = 23;  lcdltdc.vfp = 22;
      }
      else if (pre_lcdid == 0X7016)
      {
          lcdltdc.pwidth = 1024; lcdltdc.pheight = 600;
          lcdltdc.hsw = 20; lcdltdc.hbp = 140; lcdltdc.hfp = 160;
          lcdltdc.vsw = 3;  lcdltdc.vbp = 20;  lcdltdc.vfp = 12;
      }
      else if (pre_lcdid == 0X5571)
      {
          lcdltdc.pwidth = 720; lcdltdc.pheight = 1280;
          lcdltdc.hsw = 10; lcdltdc.hbp = 36; lcdltdc.hfp = 46;
          lcdltdc.vsw = 5;  lcdltdc.vbp = 5;  lcdltdc.vfp = 16;
      }
      else if (pre_lcdid == 0X4384)
      {
          lcdltdc.pwidth = 800; lcdltdc.pheight = 480;
          lcdltdc.hsw = 48; lcdltdc.hbp = 88; lcdltdc.hfp = 40;
          lcdltdc.vsw = 3;  lcdltdc.vbp = 32;  lcdltdc.vfp = 13;
      }
      else if (pre_lcdid == 0X8081)
      {
          lcdltdc.pwidth = 800; lcdltdc.pheight = 1280;
          lcdltdc.hsw = 5; lcdltdc.hbp = 20;  lcdltdc.hfp = 40;
          lcdltdc.vsw = 3; lcdltdc.vbp = 20;  lcdltdc.vfp = 30;
      }
      else if (pre_lcdid == 0X1018)
      {
          lcdltdc.pwidth = 1280; lcdltdc.pheight = 800;
          lcdltdc.hsw = 10; lcdltdc.hbp = 140; lcdltdc.hfp = 10;
          lcdltdc.vsw = 3;  lcdltdc.vbp = 10;  lcdltdc.vfp = 10;
      }
      else
      {
          /* Unknown panel — use CubeMX defaults */
          lcdltdc.pwidth = 800; lcdltdc.pheight = 480;
          lcdltdc.hsw = 4; lcdltdc.hbp = 8; lcdltdc.hfp = 8;
          lcdltdc.vsw = 4; lcdltdc.vbp = 8; lcdltdc.vfp = 8;
      }

      lcddev.width  = lcdltdc.pwidth;
      lcddev.height = lcdltdc.pheight;
      lcdltdc.pixformat = LTDC_PIXFORMAT;
      lcdltdc.pixsize = 2;  /* RGB565 = 2 bytes per pixel */

      g_ltdc_framebuf[0] = (uint32_t *)LTDC_FRAME_BUF_ADDR;
      g_ltdc_framebuf[1] = (uint32_t *)LTDC_FRAME_BUF_LAYER1_ADDR;

      /* Override CubeMX hltdc.Init with panel-specific timing BEFORE HAL_LTDC_Init */
      if (pre_lcdid != 0)
      {
          hltdc.Init.HorizontalSync     = lcdltdc.hsw - 1;
          hltdc.Init.VerticalSync       = lcdltdc.vsw - 1;
          hltdc.Init.AccumulatedHBP     = lcdltdc.hsw + lcdltdc.hbp - 1;
          hltdc.Init.AccumulatedVBP     = lcdltdc.vsw + lcdltdc.vbp - 1;
          hltdc.Init.AccumulatedActiveW = lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth - 1;
          hltdc.Init.AccumulatedActiveH = lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight - 1;
          hltdc.Init.TotalWidth         = lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth + lcdltdc.hfp - 1;
          hltdc.Init.TotalHeigh         = lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight + lcdltdc.vfp - 1;

          if (pre_lcdid == 0X8081)
              hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AH;
          else
              hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;

          hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
          hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;

          if (pre_lcdid == 0X1018 || pre_lcdid == 0X8081)
              hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IIPC;
          else
              hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
      }
  }
  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IIPC;
  hltdc.Init.HorizontalSync = 3;
  hltdc.Init.VerticalSync = 3;
  hltdc.Init.AccumulatedHBP = 11;
  hltdc.Init.AccumulatedVBP = 11;
  hltdc.Init.AccumulatedActiveW = 811;
  hltdc.Init.AccumulatedActiveH = 491;
  hltdc.Init.TotalWidth = 819;
  hltdc.Init.TotalHeigh = 499;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 800;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 480;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0xD0000000;
  pLayerCfg.ImageWidth = 800;
  pLayerCfg.ImageHeight = 480;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */
  /* Panel detection & timing already done in USER CODE BEGIN LTDC_Init 0.
   * HAL_LTDC_Init() ran with correct timing. Now just configure layers + backlight. */
  {
      uint16_t lcdid = (uint16_t)lcddev.id;

      /* Set panel-specific pixel clock (CubeMX MspInit sets PLL3R=9 by default) */
      if (lcdid == 0X4342)
          ltdc_clk_set(300, 25, 33);
      else if (lcdid == 0X7084 || lcdid == 0X4384)
          ltdc_clk_set(300, 25, 9);
      else if (lcdid == 0X7016 || lcdid == 0X5571)
          ltdc_clk_set(lcdid == 0X5571 ? 330 : 300, 25, 6);
      else if (lcdid == 0X8081 || lcdid == 0X1018)
          ltdc_clk_set(300, 25, 5);

      /* Reconfigure Layer 0 with correct framebuffer & window */
      ltdc_layer_parameter_config(0, (uint32_t)g_ltdc_framebuf[0], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);
      ltdc_layer_window_config(0, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);

      /* Layer 1 configured but disabled */
      ltdc_layer_parameter_config(1, (uint32_t)g_ltdc_framebuf[1], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);
      ltdc_layer_window_config(1, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);
      ltdc_layer_switch(1, 0);

      ltdc_display_dir(1);
      lcddev.dir = 1;          /* landscape — needed by touch driver coordinate mapping */
      ltdc_select_layer(0);

      /* Panel reset sequence (not for MIPI) */
      if (lcdid != 0X5571 && lcdid != 0)
      {
          LTDC_RST(1);
          delay_ms(10);
          LTDC_RST(0);
          delay_ms(50);
          LTDC_RST(1);
          delay_ms(200);
      }

      LTDC_BL(1);
      ltdc_clear(0XFFFFFFFF);
  }
  /* USER CODE END LTDC_Init 2 */

}

void HAL_LTDC_MspInit(LTDC_HandleTypeDef* ltdcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(ltdcHandle->Instance==LTDC)
  {
  /* USER CODE BEGIN LTDC_MspInit 0 */

  /* USER CODE END LTDC_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLL3.PLL3M = 25;
    PeriphClkInitStruct.PLL3.PLL3N = 300;
    PeriphClkInitStruct.PLL3.PLL3P = 2;
    PeriphClkInitStruct.PLL3.PLL3Q = 2;
    PeriphClkInitStruct.PLL3.PLL3R = 9;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0.0;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* LTDC clock enable */
    __HAL_RCC_LTDC_CLK_ENABLE();

    __HAL_RCC_GPIOK_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    /**LTDC GPIO Configuration
    PK5     ------> LTDC_B6
    PK4     ------> LTDC_B5
    PJ15     ------> LTDC_B3
    PK6     ------> LTDC_B7
    PK3     ------> LTDC_B4
    PK7     ------> LTDC_DE
    PJ14     ------> LTDC_B2
    PJ12     ------> LTDC_B0
    PJ13     ------> LTDC_B1
    PI12     ------> LTDC_HSYNC
    PI13     ------> LTDC_VSYNC
    PI14     ------> LTDC_CLK
    PK2     ------> LTDC_G7
    PK0     ------> LTDC_G5
    PK1     ------> LTDC_G6
    PJ11     ------> LTDC_G4
    PJ10     ------> LTDC_G3
    PJ9     ------> LTDC_G2
    PJ0     ------> LTDC_R1
    PJ8     ------> LTDC_G1
    PJ7     ------> LTDC_G0
    PJ6     ------> LTDC_R7
    PI15     ------> LTDC_R0
    PJ1     ------> LTDC_R2
    PJ5     ------> LTDC_R6
    PJ2     ------> LTDC_R3
    PJ3     ------> LTDC_R4
    PJ4     ------> LTDC_R5
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_3
                          |GPIO_PIN_7|GPIO_PIN_2|GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_11|GPIO_PIN_10|GPIO_PIN_9|GPIO_PIN_0
                          |GPIO_PIN_8|GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_1
                          |GPIO_PIN_5|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

    /* LTDC interrupt Init */
    HAL_NVIC_SetPriority(LTDC_IRQn, 9, 0);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);
  /* USER CODE BEGIN LTDC_MspInit 1 */

  /* USER CODE END LTDC_MspInit 1 */
  }
}

void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef* ltdcHandle)
{

  if(ltdcHandle->Instance==LTDC)
  {
  /* USER CODE BEGIN LTDC_MspDeInit 0 */

  /* USER CODE END LTDC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LTDC_CLK_DISABLE();

    /**LTDC GPIO Configuration
    PK5     ------> LTDC_B6
    PK4     ------> LTDC_B5
    PJ15     ------> LTDC_B3
    PK6     ------> LTDC_B7
    PK3     ------> LTDC_B4
    PK7     ------> LTDC_DE
    PJ14     ------> LTDC_B2
    PJ12     ------> LTDC_B0
    PJ13     ------> LTDC_B1
    PI12     ------> LTDC_HSYNC
    PI13     ------> LTDC_VSYNC
    PI14     ------> LTDC_CLK
    PK2     ------> LTDC_G7
    PK0     ------> LTDC_G5
    PK1     ------> LTDC_G6
    PJ11     ------> LTDC_G4
    PJ10     ------> LTDC_G3
    PJ9     ------> LTDC_G2
    PJ0     ------> LTDC_R1
    PJ8     ------> LTDC_G1
    PJ7     ------> LTDC_G0
    PJ6     ------> LTDC_R7
    PI15     ------> LTDC_R0
    PJ1     ------> LTDC_R2
    PJ5     ------> LTDC_R6
    PJ2     ------> LTDC_R3
    PJ3     ------> LTDC_R4
    PJ4     ------> LTDC_R5
    */
    HAL_GPIO_DeInit(GPIOK, GPIO_PIN_5|GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_3
                          |GPIO_PIN_7|GPIO_PIN_2|GPIO_PIN_0|GPIO_PIN_1);

    HAL_GPIO_DeInit(GPIOJ, GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_11|GPIO_PIN_10|GPIO_PIN_9|GPIO_PIN_0
                          |GPIO_PIN_8|GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_1
                          |GPIO_PIN_5|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4);

    HAL_GPIO_DeInit(GPIOI, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15);

    /* LTDC interrupt Deinit */
    HAL_NVIC_DisableIRQ(LTDC_IRQn);
  /* USER CODE BEGIN LTDC_MspDeInit 1 */

  /* USER CODE END LTDC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

uint8_t ltdc_clk_set(uint32_t pll3n, uint32_t pll3m, uint32_t pll3r)
{
    RCC_PeriphCLKInitTypeDef periphclk_initure;

    periphclk_initure.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    periphclk_initure.PLL3.PLL3M = pll3m;
    periphclk_initure.PLL3.PLL3N = pll3n;
    periphclk_initure.PLL3.PLL3P = 2;
    periphclk_initure.PLL3.PLL3Q = 2;
    periphclk_initure.PLL3.PLL3R = pll3r;

    if (HAL_RCCEx_PeriphCLKConfig(&periphclk_initure) == HAL_OK)
        return 0;
    else
        return 1;
}

void ltdc_layer_window_config(uint8_t layerx, uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    HAL_LTDC_SetWindowPosition(&hltdc, sx, sy, layerx);
    HAL_LTDC_SetWindowSize(&hltdc, width, height, layerx);

    if (lcdltdc.pheight == 1280 && layerx == 0)
    {
        LTDC_Layer1->CFBLR = (width * 4 << 16) | (width * 4 + 7);
    }
}

void ltdc_layer_parameter_config(uint8_t layerx, uint32_t bufaddr, uint8_t pixformat, uint8_t alpha, uint8_t alpha0, uint8_t bfac1, uint8_t bfac2, uint32_t bkcolor)
{
    LTDC_LayerCfgTypeDef playercfg;

    playercfg.WindowX0 = 0;
    playercfg.WindowY0 = 0;
    playercfg.WindowX1 = lcdltdc.pwidth;
    playercfg.WindowY1 = lcdltdc.pheight;
    playercfg.PixelFormat = pixformat;
    playercfg.Alpha = alpha;
    playercfg.Alpha0 = alpha0;
    playercfg.BlendingFactor1 = (uint32_t)bfac1 << 8;
    playercfg.BlendingFactor2 = (uint32_t)bfac2;
    playercfg.FBStartAdress = bufaddr;
    playercfg.ImageWidth = lcdltdc.pwidth;
    playercfg.ImageHeight = lcdltdc.pheight;
    playercfg.Backcolor.Red = (uint8_t)(bkcolor & 0X00FF0000) >> 16;
    playercfg.Backcolor.Green = (uint8_t)(bkcolor & 0X0000FF00) >> 8;
    playercfg.Backcolor.Blue = (uint8_t)bkcolor & 0X000000FF;
    HAL_LTDC_ConfigLayer(&hltdc, &playercfg, layerx);
}

uint16_t ltdc_panelid_read(void)
{
    uint8_t idx = 0;
    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    gpio_init_struct.Pin = GPIO_PIN_6;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOJ, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_2 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOK, &gpio_init_struct);

    delay_us(10);
    idx  = (uint8_t)HAL_GPIO_ReadPin(GPIOJ, GPIO_PIN_6);
    idx |= (uint8_t)HAL_GPIO_ReadPin(GPIOK, GPIO_PIN_2) << 1;
    idx |= (uint8_t)HAL_GPIO_ReadPin(GPIOK, GPIO_PIN_6) << 2;

    switch (idx)
    {
        case 0: return 0X4342;
        case 1: return 0X7084;
        case 2: return 0X7016;
        case 3: return 0X5571;
        case 4: return 0X4384;
        case 5: return 0X1018;
        default: return 0;
    }
}

/* Compatibility wrapper — called from lcd_init() */
void ltdc_init(void)
{
    MX_LTDC_Init();
}

/* USER CODE END 1 */

