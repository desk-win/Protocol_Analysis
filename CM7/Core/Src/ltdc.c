#include "ltdc.h"

/* USER CODE BEGIN 0 */
#include "ltdc_draw.h"
#include "delay.h"
#include "tft_spi.h"
#include "lcd.h"
/* USER CODE END 0 */

LTDC_HandleTypeDef hltdc;

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

/* LTDC init function */
void MX_LTDC_Init(void)
{
    uint16_t lcdid = 0;

    lcdid = ltdc_panelid_read();

    if (lcdid == 0X5571)
        tft_spi_init();

#if RGB_80_8001280
    lcdid = 0X8081;
#endif

    if (lcdid == 0X4342)
    {
        lcdltdc.pwidth = 480; lcdltdc.pheight = 272;
        lcdltdc.hsw = 1; lcdltdc.hbp = 40; lcdltdc.hfp = 5;
        lcdltdc.vsw = 1; lcdltdc.vbp = 8;  lcdltdc.vfp = 8;
        ltdc_clk_set(300, 25, 33);
    }
    else if (lcdid == 0X7084)
    {
        lcdltdc.pwidth = 800; lcdltdc.pheight = 480;
        lcdltdc.hsw = 1; lcdltdc.hbp = 46;  lcdltdc.hfp = 210;
        lcdltdc.vsw = 1; lcdltdc.vbp = 23;  lcdltdc.vfp = 22;
        ltdc_clk_set(300, 25, 9);
    }
    else if (lcdid == 0X7016)
    {
        lcdltdc.pwidth = 1024; lcdltdc.pheight = 600;
        lcdltdc.hsw = 20; lcdltdc.hbp = 140; lcdltdc.hfp = 160;
        lcdltdc.vsw = 3;  lcdltdc.vbp = 20;  lcdltdc.vfp = 12;
        ltdc_clk_set(300, 25, 6);
    }
    else if (lcdid == 0X5571)
    {
        lcdltdc.pwidth = 720; lcdltdc.pheight = 1280;
        lcdltdc.hsw = 10; lcdltdc.hbp = 36; lcdltdc.hfp = 46;
        lcdltdc.vsw = 5;  lcdltdc.vbp = 5;  lcdltdc.vfp = 16;
        ltdc_clk_set(330, 25, 6);
    }
    else if (lcdid == 0X4384)
    {
        lcdltdc.pwidth = 800; lcdltdc.pheight = 480;
        lcdltdc.hsw = 48; lcdltdc.hbp = 88; lcdltdc.hfp = 40;
        lcdltdc.vsw = 3;  lcdltdc.vbp = 32; lcdltdc.vfp = 13;
        ltdc_clk_set(300, 25, 9);
    }
    else if (lcdid == 0X8081)
    {
        lcdltdc.pwidth = 800; lcdltdc.pheight = 1280;
        lcdltdc.hsw = 5; lcdltdc.hbp = 20;  lcdltdc.hfp = 40;
        lcdltdc.vsw = 3; lcdltdc.vbp = 20;  lcdltdc.vfp = 30;
        ltdc_clk_set(300, 25, 5);
    }
    else if (lcdid == 0X1018)
    {
        lcdltdc.pwidth = 1280; lcdltdc.pheight = 800;
        lcdltdc.hsw = 10; lcdltdc.hbp = 140; lcdltdc.hfp = 10;
        lcdltdc.vsw = 3;  lcdltdc.vbp = 10;  lcdltdc.vfp = 10;
        ltdc_clk_set(300, 25, 5);
    }

    lcddev.width = lcdltdc.pwidth;
    lcddev.height = lcdltdc.pheight;
    lcdltdc.pixformat = LTDC_PIXFORMAT;

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
    g_ltdc_framebuf[0] = (uint32_t *)LTDC_FRAME_BUF_ADDR;
    g_ltdc_framebuf[1] = (uint32_t *)LTDC_FRAME_BUF_LAYER1_ADDR;
    lcdltdc.pixsize = 4;
#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    g_ltdc_framebuf[0] = (uint32_t *)LTDC_FRAME_BUF_ADDR;
    g_ltdc_framebuf[1] = (uint32_t *)LTDC_FRAME_BUF_LAYER1_ADDR;
    lcdltdc.pixsize = 3;
#else
    g_ltdc_framebuf[0] = (uint32_t *)LTDC_FRAME_BUF_ADDR;
    g_ltdc_framebuf[1] = (uint32_t *)LTDC_FRAME_BUF_LAYER1_ADDR;
    lcdltdc.pixsize = 2;
#endif

    /* LTDC init */
    hltdc.Instance = LTDC;

    if (lcdid == 0X8081)
        hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AH;
    else
        hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;

    hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
    hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
    hltdc.State = HAL_LTDC_STATE_RESET;

    if (lcdid == 0X1018 || lcdid == 0X8081)
        hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IIPC;
    else
        hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

    hltdc.Init.HorizontalSync = lcdltdc.hsw - 1;
    hltdc.Init.VerticalSync = lcdltdc.vsw - 1;
    hltdc.Init.AccumulatedHBP = lcdltdc.hsw + lcdltdc.hbp - 1;
    hltdc.Init.AccumulatedVBP = lcdltdc.vsw + lcdltdc.vbp - 1;
    hltdc.Init.AccumulatedActiveW = lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth - 1;
    hltdc.Init.AccumulatedActiveH = lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight - 1;
    hltdc.Init.TotalWidth = lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth + lcdltdc.hfp - 1;
    hltdc.Init.TotalHeigh = lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight + lcdltdc.vfp - 1;
    hltdc.Init.Backcolor.Red = 0;
    hltdc.Init.Backcolor.Green = 0;
    hltdc.Init.Backcolor.Blue = 0;
    HAL_LTDC_Init(&hltdc);

    /* Layer 0 config — background */
    ltdc_layer_parameter_config(0, (uint32_t)g_ltdc_framebuf[0], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);
    ltdc_layer_window_config(0, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);

    /* Layer 1 config — foreground (reserved for TouchGFX) */
    ltdc_layer_parameter_config(1, (uint32_t)g_ltdc_framebuf[1], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);
    ltdc_layer_window_config(1, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);
    ltdc_layer_switch(1, 0);  /* Disable Layer 1 for now — bare-metal uses Layer 0 only */

    ltdc_display_dir(1);
    ltdc_select_layer(0);

    if (lcdid != 0X5571)
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

void HAL_LTDC_MspInit(LTDC_HandleTypeDef *hltdc)
{
    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_LTDC_CLK_ENABLE();
    __HAL_RCC_DMA2D_CLK_ENABLE();

    LTDC_BL_GPIO_CLK_ENABLE();
    LTDC_RST_GPIO_CLK_ENABLE();
    LTDC_DE_GPIO_CLK_ENABLE();
    LTDC_VSYNC_GPIO_CLK_ENABLE();
    LTDC_HSYNC_GPIO_CLK_ENABLE();
    LTDC_CLK_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = LTDC_BL_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LTDC_BL_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LTDC_RST_GPIO_PIN;
    HAL_GPIO_Init(LTDC_RST_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LTDC_DE_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init_struct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(LTDC_DE_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LTDC_VSYNC_GPIO_PIN;
    HAL_GPIO_Init(LTDC_VSYNC_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LTDC_HSYNC_GPIO_PIN;
    HAL_GPIO_Init(LTDC_HSYNC_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LTDC_CLK_GPIO_PIN;
    HAL_GPIO_Init(LTDC_CLK_GPIO_PORT, &gpio_init_struct);

    /* LTDC RGB data pins */
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    gpio_init_struct.Pin = GPIO_PIN_15;
    gpio_init_struct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOI, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_All;
    HAL_GPIO_Init(GPIOJ, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOK, &gpio_init_struct);
}

void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef* ltdcHandle)
{
    if(ltdcHandle->Instance==LTDC)
    {
        __HAL_RCC_LTDC_CLK_DISABLE();

        HAL_GPIO_DeInit(GPIOK, GPIO_PIN_5|GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_3
                              |GPIO_PIN_7|GPIO_PIN_2|GPIO_PIN_0|GPIO_PIN_1);

        HAL_GPIO_DeInit(GPIOJ, GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_12|GPIO_PIN_13
                              |GPIO_PIN_11|GPIO_PIN_10|GPIO_PIN_9|GPIO_PIN_0
                              |GPIO_PIN_8|GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_1
                              |GPIO_PIN_5|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4);

        HAL_GPIO_DeInit(GPIOI, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15);
    }
}
