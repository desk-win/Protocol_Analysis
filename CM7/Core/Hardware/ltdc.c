#include "ltdc.h"
#include "lcd.h"
#include "delay.h"
#include "tft_spi.h"

LTDC_HandleTypeDef  g_ltdc_handle;
DMA2D_HandleTypeDef g_dma2d_handle;

/* Frame buffer is placed in SDRAM at 0xD0000000.
 * SDRAM is 32MB, already accessible after sdram_init().
 * No static allocation needed - use the memory directly.
 */
uint32_t *g_ltdc_framebuf[2];
_ltdc_dev lcdltdc;

void ltdc_switch(uint8_t sw)
{
    if (sw)
        __HAL_LTDC_ENABLE(&g_ltdc_handle);
    else
        __HAL_LTDC_DISABLE(&g_ltdc_handle);
}

void ltdc_layer_switch(uint8_t layerx, uint8_t sw)
{
    if (sw)
        __HAL_LTDC_LAYER_ENABLE(&g_ltdc_handle, layerx);
    else
        __HAL_LTDC_LAYER_DISABLE(&g_ltdc_handle, layerx);

    __HAL_LTDC_RELOAD_CONFIG(&g_ltdc_handle);
}

void ltdc_select_layer(uint8_t layerx)
{
    lcdltdc.activelayer = layerx;
}

void ltdc_display_dir(uint8_t dir)
{
    lcdltdc.dir = dir;

    if (dir == 0)
    {
        lcdltdc.width = lcdltdc.pheight;
        lcdltdc.height = lcdltdc.pwidth;
    }
    else if (dir == 1)
    {
        lcdltdc.width = lcdltdc.pwidth;
        lcdltdc.height = lcdltdc.pheight;
    }
}

void ltdc_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
    if (lcdltdc.dir)
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    else
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    if (lcdltdc.dir)
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
        *(uint8_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x) + 2) = color >> 16;
    }
    else
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
        *(uint8_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y) + 2) = color >> 16;
    }
#else
    if (lcdltdc.dir)
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    else
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
#endif
}

uint32_t ltdc_read_point(uint16_t x, uint16_t y)
{
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
    if (lcdltdc.dir)
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    else
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    if (lcdltdc.dir)
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) & 0XFFFFFF;
    else
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) & 0XFFFFFF;
#else
    if (lcdltdc.dir)
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    else
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
#endif
}

void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint32_t psx, psy, pex, pey;
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    if (lcdltdc.dir)
    {
        psx = sx; psy = sy; pex = ex; pey = ey;
    }
    else
    {
        if (ex >= lcdltdc.pheight) ex = lcdltdc.pheight - 1;
        if (sx >= lcdltdc.pheight) sx = lcdltdc.pheight - 1;
        psx = sy; psy = lcdltdc.pheight - ex - 1;
        pex = ey; pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));

    __HAL_RCC_DMA2D_CLK_ENABLE();

    DMA2D->CR &= ~(DMA2D_CR_START);
    DMA2D->CR = DMA2D_R2M;
    DMA2D->OPFCCR = LTDC_PIXFORMAT;
    DMA2D->OOR = offline;
    DMA2D->OMAR = addr;
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16);
    DMA2D->OCOLR = color;
    DMA2D->CR |= DMA2D_CR_START;

    while ((DMA2D->ISR & (DMA2D_FLAG_TC)) == 0)
    {
        timeout++;
        if (timeout > 0X1FFFFF) break;
    }

    DMA2D->IFCR |= DMA2D_FLAG_TC;
}

void ltdc_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint32_t psx, psy, pex, pey;
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    if (lcdltdc.dir)
    {
        psx = sx; psy = sy; pex = ex; pey = ey;
    }
    else
    {
        psx = sy; psy = lcdltdc.pheight - ex - 1;
        pex = ey; pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));

    __HAL_RCC_DMA2D_CLK_ENABLE();

    DMA2D->CR &= ~(DMA2D_CR_START);
    DMA2D->CR = DMA2D_M2M;
    DMA2D->FGPFCCR = LTDC_PIXFORMAT;
    DMA2D->FGOR = 0;
    DMA2D->OOR = offline;
    DMA2D->FGMAR = (uint32_t)color;
    DMA2D->OMAR = addr;
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16);
    DMA2D->CR |= DMA2D_CR_START;

    while((DMA2D->ISR & (DMA2D_FLAG_TC)) == 0)
    {
        timeout++;
        if (timeout > 0X1FFFFF) break;
    }

    DMA2D->IFCR |= DMA2D_FLAG_TC;
}

void ltdc_clear(uint32_t color)
{
    ltdc_fill(0, 0, lcdltdc.width - 1, lcdltdc.height - 1, color);
}

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
    HAL_LTDC_SetWindowPosition(&g_ltdc_handle, sx, sy, layerx);
    HAL_LTDC_SetWindowSize(&g_ltdc_handle, width, height, layerx);

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
    HAL_LTDC_ConfigLayer(&g_ltdc_handle, &playercfg, layerx);
}

uint16_t ltdc_panelid_read(void)
{
    uint8_t idx = 0;
    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    gpio_init_struct.Pin = GPIO_PIN_6;                      /* R7 → PJ6 */
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOJ, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_2 | GPIO_PIN_6;         /* G7,B7 → PK2,6 */
    HAL_GPIO_Init(GPIOK, &gpio_init_struct);

    delay_us(10);
    idx  = (uint8_t)HAL_GPIO_ReadPin(GPIOJ, GPIO_PIN_6);    /* M0 */
    idx |= (uint8_t)HAL_GPIO_ReadPin(GPIOK, GPIO_PIN_2) << 1; /* M1 */
    idx |= (uint8_t)HAL_GPIO_ReadPin(GPIOK, GPIO_PIN_6) << 2; /* M2 */

    switch (idx)
    {
        case 0: return 0X4342;    /* 4.3" 480x272 */
        case 1: return 0X7084;    /* 7"   800x480 */
        case 2: return 0X7016;    /* 7"   1024x600 */
        case 3: return 0X5571;    /* 5.5" 720x1280 */
        case 4: return 0X4384;    /* 4.3" 800x480 */
        case 5: return 0X1018;    /* 10.1" 1280x800 */
        default: return 0;
    }
}

void ltdc_init(void)
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
        ltdc_clk_set(300, 25, 33);   /* 9MHz */
    }
    else if (lcdid == 0X7084)
    {
        lcdltdc.pwidth = 800; lcdltdc.pheight = 480;
        lcdltdc.hsw = 1; lcdltdc.hbp = 46;  lcdltdc.hfp = 210;
        lcdltdc.vsw = 1; lcdltdc.vbp = 23;  lcdltdc.vfp = 22;
        ltdc_clk_set(300, 25, 9);    /* 33MHz */
    }
    else if (lcdid == 0X7016)
    {
        lcdltdc.pwidth = 1024; lcdltdc.pheight = 600;
        lcdltdc.hsw = 20; lcdltdc.hbp = 140; lcdltdc.hfp = 160;
        lcdltdc.vsw = 3;  lcdltdc.vbp = 20;  lcdltdc.vfp = 12;
        ltdc_clk_set(300, 25, 6);    /* 50MHz */
    }
    else if (lcdid == 0X5571)
    {
        lcdltdc.pwidth = 720; lcdltdc.pheight = 1280;
        lcdltdc.hsw = 10; lcdltdc.hbp = 36; lcdltdc.hfp = 46;
        lcdltdc.vsw = 5;  lcdltdc.vbp = 5;  lcdltdc.vfp = 16;
        ltdc_clk_set(330, 25, 6);    /* 55MHz */
    }
    else if (lcdid == 0X4384)
    {
        lcdltdc.pwidth = 800; lcdltdc.pheight = 480;
        lcdltdc.hsw = 48; lcdltdc.hbp = 88; lcdltdc.hfp = 40;
        lcdltdc.vsw = 3;  lcdltdc.vbp = 32; lcdltdc.vfp = 13;
        ltdc_clk_set(300, 25, 9);    /* 33MHz */
    }
    else if (lcdid == 0X8081)
    {
        lcdltdc.pwidth = 800; lcdltdc.pheight = 1280;
        lcdltdc.hsw = 5; lcdltdc.hbp = 20;  lcdltdc.hfp = 40;
        lcdltdc.vsw = 3; lcdltdc.vbp = 20;  lcdltdc.vfp = 30;
        ltdc_clk_set(300, 25, 5);    /* 60MHz */
    }
    else if (lcdid == 0X1018)
    {
        lcdltdc.pwidth = 1280; lcdltdc.pheight = 800;
        lcdltdc.hsw = 10; lcdltdc.hbp = 140; lcdltdc.hfp = 10;
        lcdltdc.vsw = 3;  lcdltdc.vbp = 10;  lcdltdc.vfp = 10;
        ltdc_clk_set(300, 25, 5);    /* 60MHz */
    }

    lcddev.width = lcdltdc.pwidth;
    lcddev.height = lcdltdc.pheight;
    lcdltdc.pixformat = LTDC_PIXFORMAT;

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
    g_ltdc_framebuf[0] = (uint32_t *)LTDC_FRAME_BUF_ADDR;
    lcdltdc.pixsize = 4;
#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    g_ltdc_framebuf[0] = (uint32_t *)LTDC_FRAME_BUF_ADDR;
    lcdltdc.pixsize = 3;
#else
    g_ltdc_framebuf[0] = (uint32_t *)LTDC_FRAME_BUF_ADDR;
    lcdltdc.pixsize = 2;
#endif

    /* LTDC init */
    g_ltdc_handle.Instance = LTDC;

    if (lcdid == 0X8081)
        g_ltdc_handle.Init.HSPolarity = LTDC_HSPOLARITY_AH;
    else
        g_ltdc_handle.Init.HSPolarity = LTDC_HSPOLARITY_AL;

    g_ltdc_handle.Init.VSPolarity = LTDC_VSPOLARITY_AL;
    g_ltdc_handle.Init.DEPolarity = LTDC_DEPOLARITY_AL;
    g_ltdc_handle.State = HAL_LTDC_STATE_RESET;

    if (lcdid == 0X1018 || lcdid == 0X8081)
        g_ltdc_handle.Init.PCPolarity = LTDC_PCPOLARITY_IIPC;
    else
        g_ltdc_handle.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

    g_ltdc_handle.Init.HorizontalSync = lcdltdc.hsw - 1;
    g_ltdc_handle.Init.VerticalSync = lcdltdc.vsw - 1;
    g_ltdc_handle.Init.AccumulatedHBP = lcdltdc.hsw + lcdltdc.hbp - 1;
    g_ltdc_handle.Init.AccumulatedVBP = lcdltdc.vsw + lcdltdc.vbp - 1;
    g_ltdc_handle.Init.AccumulatedActiveW = lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth - 1;
    g_ltdc_handle.Init.AccumulatedActiveH = lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight - 1;
    g_ltdc_handle.Init.TotalWidth = lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth + lcdltdc.hfp - 1;
    g_ltdc_handle.Init.TotalHeigh = lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight + lcdltdc.vfp - 1;
    g_ltdc_handle.Init.Backcolor.Red = 0;
    g_ltdc_handle.Init.Backcolor.Green = 0;
    g_ltdc_handle.Init.Backcolor.Blue = 0;
    HAL_LTDC_Init(&g_ltdc_handle);

    /* Layer config */
    ltdc_layer_parameter_config(0, (uint32_t)g_ltdc_framebuf[0], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);
    ltdc_layer_window_config(0, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);

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
