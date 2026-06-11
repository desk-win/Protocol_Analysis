#include <gui/screen_screen/screenView.hpp>
#include <touchgfx/Color.hpp>
#include <touch.h>
#include <bsp_driver_sd.h>
#include <sdmmc.h>
#include <main.h>
#include <fatfs.h>

/*
 * sdnand_init() — copied from experiment 30 (sdmmc_sdnand.c)
 * The ONLY changes: uses hsd2 instead of g_sdnand_handle.
 *
 * This does GPIO + clock config manually, then calls HAL_SD_Init()
 * exactly like the working bare-metal example.
 */
static uint8_t sdnand_init(void)
{
    uint8_t SD_Error;
    GPIO_InitTypeDef gpio_init_struct;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /* Configure SDMMC kernel clock source (PLL1_Q) */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
    PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

    /* DeInit FIRST (resets handle state, MspDeInit is now empty) */
    HAL_SD_DeInit(&hsd2);

    /* THEN configure GPIO + clocks */
    __HAL_RCC_SDMMC2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PB14 = D0 */
    gpio_init_struct.Pin = GPIO_PIN_14;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init_struct.Alternate = GPIO_AF9_SDMMC2;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);

    /* PD6 = CLK */
    gpio_init_struct.Pin = GPIO_PIN_6;
    gpio_init_struct.Alternate = GPIO_AF11_SDMMC2;
    HAL_GPIO_Init(GPIOD, &gpio_init_struct);

    /* PA0 = CMD */
    gpio_init_struct.Pin = GPIO_PIN_0;
    gpio_init_struct.Alternate = GPIO_AF9_SDMMC2;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    hsd2.Instance = SDMMC2;
    hsd2.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd2.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd2.Init.BusWide = SDMMC_BUS_WIDE_4B;
    hsd2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd2.Init.ClockDiv = SDMMC_NSpeed_CLK_DIV;

    SD_Error = HAL_SD_Init(&hsd2);

    if (SD_Error != HAL_OK)
    {
        /* Encode: HAL_ERROR=1, HAL_TIMEOUT=2, HAL_BUSY=3,
         *         MspInit fail=4, PowerON fail=5, InitCard fail=6
         * Plus top 8 bits = hsd2.ErrorCode >> 25 (crude hash)  */
        uint8_t base;
        if (SD_Error == HAL_ERROR)   base = 1;
        else if (SD_Error == HAL_TIMEOUT) base = 2;
        else if (SD_Error == HAL_BUSY)    base = 3;
        else base = 4;

        return base;
    }

    return 0;
}

/**
 * @brief  SD NAND 综合测试（面向ADC波形存储场景）
 *
 * 测试流程：
 *   1. f_mount
 *   2. 写入测试文件 (模拟ADC采样数据: 8KB)
 *   3. 读回并校验数据完整性
 *   4. 获取卡容量信息
 *
 * @retval 0=全部通过
 *         1=mount失败
 *         2=写文件失败
 *         3=读文件失败
 *         4=数据校验失败
 *         5=获取容量失败
 */
static int sd_nand_full_test(void)
{
    FRESULT fr;
    FIL file;
    UINT bw, br;

    /* Step 1: Mount */
    fr = f_mount(&SDFatFS, SDPath, 1);
    if (fr != FR_OK) return 1;

    /* Step 2: 写入测试文件 — 模拟ADC采样数据 */
    #define TEST_DATA_SIZE  (8 * 1024)  /* 8KB, 模拟一次ADC采样块 */
    static uint8_t wr_buf[512];
    static uint8_t rd_buf[512];

    /* 填充伪ADC数据: 递增计数 + 固定pattern */
    for (int i = 0; i < 512; i++)
    {
        wr_buf[i] = (uint8_t)(i & 0xFF);
    }

    fr = f_open(&file, "0:adc_test.bin", FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return 2;

    /* 写入16个block = 8KB */
    for (int blk = 0; blk < (TEST_DATA_SIZE / 512); blk++)
    {
        /* 每个block稍微变化pattern, 模拟不同采样段 */
        wr_buf[0] = (uint8_t)(blk & 0xFF);       /* 块编号 */
        wr_buf[1] = (uint8_t)((blk >> 8) & 0xFF);
        wr_buf[510] = (uint8_t)~(blk & 0xFF);     /* 尾部校验 */
        wr_buf[511] = 0xA5;

        fr = f_write(&file, wr_buf, 512, &bw);
        if (fr != FR_OK || bw != 512)
        {
            f_close(&file);
            return 2;
        }
    }
    f_close(&file);

    /* Step 3: 读回并校验 */
    fr = f_open(&file, "0:adc_test.bin", FA_READ);
    if (fr != FR_OK) return 3;

    for (int blk = 0; blk < (TEST_DATA_SIZE / 512); blk++)
    {
        fr = f_read(&file, rd_buf, 512, &br);
        if (fr != FR_OK || br != 512)
        {
            f_close(&file);
            return 3;
        }

        /* 校验: header */
        if (rd_buf[0] != (uint8_t)(blk & 0xFF))     { f_close(&file); return 4; }
        if (rd_buf[1] != (uint8_t)((blk >> 8) & 0xFF)){ f_close(&file); return 4; }
        /* 校验: 尾部 */
        if (rd_buf[510] != (uint8_t)~(blk & 0xFF))   { f_close(&file); return 4; }
        if (rd_buf[511] != 0xA5)                       { f_close(&file); return 4; }
        /* 校验: 中间数据 */
        for (int i = 2; i < 510; i++)
        {
            if (rd_buf[i] != (uint8_t)(i & 0xFF))
            {
                f_close(&file);
                return 4;
            }
        }
    }
    f_close(&file);

    /* Step 4: 获取卡容量 (验证ioctl) */
    FATFS *fs;
    DWORD fre_clust;
    fr = f_getfree("0:", &fre_clust, &fs);
    if (fr != FR_OK) return 5;

    /* 更新全局卡信息供UI显示 */
    g_sd_card_info.LogBlockNbr = (fs->n_fatent - 2) * fs->csize;
    g_sd_card_info.LogBlockSize = 512;
    g_sd_free_kb = fre_clust * fs->csize / 2;  /* 空闲空间KB */

    return 0;
}

screenView::screenView()
    : touchDotAdded(false), sdTestDone(false), sdTestResult(-1),
      fatfsResult(-1), tickCount(0), sdStatusBarAdded(false),
      capacityTextAdded(false)
{
    capacityBuf[0] = 0;
}

void screenView::setupScreen()
{
    screenViewBase::setupScreen();

    touchDot.setPosition(0, 0, 32, 32);
    touchDot.setCenter(16, 16);
    touchDot.setRadius(15);
    touchDot.setLineWidth(0);
    touchDot.setArc(0, 360);
    touchDotPainter.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    touchDot.setPainter(touchDotPainter);
    touchDot.setVisible(false);
    touchDot.setTouchable(false);

    sdStatusBar.setPosition(0, 460, 200, 8);
    sdStatusBar.setVisible(false);

    /* SD容量文字 */
    capacityText.setPosition(210, 452, 400, 24);
    capacityText.setTypedText(TypedText(0));   /* __TXT_WILDCARD = Verdana20 */
    capacityText.setWildcard(capacityBuf);
    capacityText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    capacityText.setVisible(false);
    capacityTextAdded = false;
}

void screenView::tearDownScreen()
{
    screenViewBase::tearDownScreen();
}

void screenView::handleTickEvent()
{
    screenViewBase::handleTickEvent();

    if (!sdTestDone)
    {
        tickCount++;

        if (tickCount == 1)
        {
            sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 255, 255));
            sdStatusBar.setVisible(true);
            if (!sdStatusBarAdded) { add(sdStatusBar); sdStatusBarAdded = true; }
            sdStatusBar.invalidate();
        }

        if (tickCount >= 60)
        {
            sdTestDone = true;

            uint8_t det = sdnand_init();

            if (det == 0)
            {
                fatfsResult = sd_nand_full_test();
                sdTestResult = (fatfsResult == 0) ? 0 : 10 + fatfsResult;
            }
            else
            {
                sdTestResult = det;
            }

            /* Color = diagnostic */
            if (sdTestResult == 0)
                sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 255, 0));         /* GREEN  = 全部通过 */
            else if (sdTestResult >= 10)
            {
                /* FatFs/读写测试失败 */
                switch (sdTestResult - 10)
                {
                case 1:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 255, 0));   break; /* YELLOW  = mount失败 */
                case 2:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 165, 0));   break; /* ORANGE  = 写文件失败 */
                case 3:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 0, 255));     break; /* BLUE    = 读文件失败 */
                case 4:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 0, 255));   break; /* MAGENTA = 数据校验失败! */
                case 5:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(128, 128, 128)); break; /* GRAY    = 获取容量失败 */
                default: sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));     break; /* RED */
                }
            }
            else
            {
                /* SD NAND init 失败 */
                switch (sdTestResult)
                {
                case 1:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 255, 0));   break; /* YELLOW  = HAL_ERROR */
                case 2:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 255, 255));   break; /* CYAN    = HAL_TIMEOUT */
                default: sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));     break; /* RED */
                }
            }
            sdStatusBar.invalidate();

            if (!touchDotAdded) { add(touchDot); touchDotAdded = true; }
            touchDotPainter.setColor(
                (sdTestResult == 0) ? touchgfx::Color::getColorFromRGB(0, 255, 0)
                                    : touchgfx::Color::getColorFromRGB(255, 0, 0));
            touchDot.setPainter(touchDotPainter);
            touchDot.setRadius(8);
            touchDot.setCenter(8, 8);
            touchDot.setPosition(460, 460, 16, 16);
            touchDot.setVisible(true);
            touchDot.invalidate();

            /* 显示容量文字 */
            if (sdTestResult == 0)
            {
                uint32_t total_kb = g_sd_card_info.LogBlockNbr / 2;
                uint32_t free_kb_val = g_sd_free_kb;
                touchgfx::Unicode::UnicodeChar* p = capacityBuf;
                const char* s1 = "Total:";
                while (*s1) *p++ = *s1++;
                /* total MB */
                {
                    char tmp[12];
                    int len = 0;
                    uint32_t v = total_kb / 1024;
                    do { tmp[len++] = '0' + (v % 10); v /= 10; } while (v);
                    for (int i = len - 1; i >= 0; i--) *p++ = tmp[i];
                }
                const char* s2 = "MB Free:";
                while (*s2) *p++ = *s2++;
                {
                    char tmp[12];
                    int len = 0;
                    uint32_t v = free_kb_val / 1024;
                    do { tmp[len++] = '0' + (v % 10); v /= 10; } while (v);
                    for (int i = len - 1; i >= 0; i--) *p++ = tmp[i];
                }
                const char* s3 = "MB";
                while (*s3) *p++ = *s3++;
                *p = 0;

                capacityText.invalidate();
                capacityText.setWildcard(capacityBuf);
                capacityText.setVisible(true);
                if (!capacityTextAdded)
                {
                    add(capacityText);
                    capacityTextAdded = true;
                }
                capacityText.invalidate();
            }
        }
    }

    if (tp_dev.sta & TP_PRES_DOWN)
    {
        uint16_t tx = tp_dev.x[0];
        uint16_t ty = tp_dev.y[0];
        if (tx < 800 && ty < 480)
        {
            touchDot.invalidate();
            touchDot.setRadius(15);
            touchDot.setCenter(16, 16);
            touchDot.setPosition(tx - 16, ty - 16, 32, 32);
            touchDot.setVisible(true);
            touchDot.invalidate();
        }
    }
    else if (sdTestDone)
    {
        touchDot.invalidate();
        touchDot.setRadius(8);
        touchDot.setCenter(8, 8);
        touchDot.setPosition(460, 460, 16, 16);
        touchDot.setVisible(true);
        touchDot.invalidate();
    }
}
