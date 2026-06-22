#include <gui/sd_test_screen_screen/SD_Test_ScreenView.hpp>
#include <touchgfx/Color.hpp>
#include <stdio.h>
#include <touch.h>
#include <bsp_driver_sd.h>
#include <sdmmc.h>
#include <main.h>
#include <fatfs.h>

/*
 * sdnand_init() — copied from experiment 30 (sdmmc_sdnand.c)
 * The ONLY changes: uses hsd2 instead of g_sdnand_handle.
 */
extern "C" uint8_t sdnand_init(void)
{
    uint8_t SD_Error;
    GPIO_InitTypeDef gpio_init_struct;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
    PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

    HAL_SD_DeInit(&hsd2);

    __HAL_RCC_SDMMC2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

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

    gpio_init_struct.Pin = GPIO_PIN_6;
    gpio_init_struct.Alternate = GPIO_AF11_SDMMC2;
    HAL_GPIO_Init(GPIOD, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0;
    gpio_init_struct.Alternate = GPIO_AF9_SDMMC2;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    hsd2.Instance = SDMMC2;
    hsd2.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd2.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd2.Init.BusWide = SDMMC_BUS_WIDE_4B;
    /* 开硬件流控：SD NAND 偶发读取错误（Free 每次不同值）部分源于 FIFO 溢出，
     * 流控让 SDMMC 在 FIFO 满时自动暂停 CLK，防数据丢/错。 */
    hsd2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_ENABLE;
    hsd2.Init.ClockDiv = SDMMC_NSpeed_CLK_DIV;

    SD_Error = HAL_SD_Init(&hsd2);

    if (SD_Error != HAL_OK)
    {
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
 * @retval 0=全部通过, 1=mount失败, 2=写文件失败,
 *         3=读文件失败, 4=数据校验失败, 5=获取容量失败
 */
static int sd_nand_full_test(void)
{
    FRESULT fr;
    FIL file;
    UINT bw, br;

    fr = f_mount(&SDFatFS, SDPath, 1);
    if (fr != FR_OK) return 1;

    #define TEST_DATA_SIZE  (8 * 1024)
    static uint8_t wr_buf[512];
    static uint8_t rd_buf[512];

    for (int i = 0; i < 512; i++)
        wr_buf[i] = (uint8_t)(i & 0xFF);

    fr = f_open(&file, "0:adc_test.bin", FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return 2;

    for (int blk = 0; blk < (TEST_DATA_SIZE / 512); blk++)
    {
        wr_buf[0] = (uint8_t)(blk & 0xFF);
        wr_buf[1] = (uint8_t)((blk >> 8) & 0xFF);
        wr_buf[510] = (uint8_t)~(blk & 0xFF);
        wr_buf[511] = 0xA5;

        fr = f_write(&file, wr_buf, 512, &bw);
        if (fr != FR_OK || bw != 512)
        {
            f_close(&file);
            return 2;
        }
    }
    f_close(&file);

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

        if (rd_buf[0] != (uint8_t)(blk & 0xFF))      { f_close(&file); return 4; }
        if (rd_buf[1] != (uint8_t)((blk >> 8) & 0xFF)) { f_close(&file); return 4; }
        if (rd_buf[510] != (uint8_t)~(blk & 0xFF))    { f_close(&file); return 4; }
        if (rd_buf[511] != 0xA5)                        { f_close(&file); return 4; }

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

    FATFS *fs;
    DWORD fre_clust;
    fr = f_getfree("0:", &fre_clust, &fs);
    if (fr != FR_OK) return 5;

    g_sd_card_info.LogBlockNbr = (fs->n_fatent - 2) * fs->csize;
    g_sd_card_info.LogBlockSize = 512;
    g_sd_free_kb = fre_clust * fs->csize / 2;

    return 0;
}

SD_Test_ScreenView::SD_Test_ScreenView()
    : touchDotAdded(false), sdTestDone(false), sdTestResult(-1),
      fatfsResult(-1), tickCount(0), sdStatusBarAdded(false)
{
    capacityBuf[0] = 0;
}

void SD_Test_ScreenView::setupScreen()
{
    SD_Test_ScreenViewBase::setupScreen();

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

    /* Bind Designer-generated textArea1 to our buffer (base's textArea1Buffer
     * is only 10 chars) and fix its size so long capacity strings aren't
     * clipped by the Designer's resizeToCurrentText() on an empty buffer. */
    textArea1.setWildcard(capacityBuf);
    textArea1.setPosition(20, 200, 760, 40);
    textArea1.invalidate();

    /* 文件列表显示区（容量下方）*/
    fileText.setPosition(20, 260, 760, 160);
    fileText.setTypedText(TypedText(T___SINGLEUSE_T387));
    fileText.setWildcard(fileBuf);
    fileText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    add(fileText);
}

void SD_Test_ScreenView::tearDownScreen()
{
    SD_Test_ScreenViewBase::tearDownScreen();
}

void SD_Test_ScreenView::handleTickEvent()
{
    SD_Test_ScreenViewBase::handleTickEvent();

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
            /* 不再 sdnand_init + sd_nand_full_test（与 defaultTask 抢 SD 导致不稳定）。
             * 等 defaultTask 完成 SD init（g_sd_status != 0），用它的结果。*/
            extern volatile uint32_t g_sd_status;
            if (g_sd_status == 0U)
            {
                /* defaultTask 还在 init，等下个 tick（sdTestDone 保持 false）*/
            }
            else
            {
                sdTestDone = true;
                sdTestResult = (g_sd_status == 4U) ? 0 : (int)g_sd_status;  /* 0=OK, 1/2/3=失败步骤 */
            }

            if (sdTestResult == 0)
                sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 255, 0));
            else if (sdTestResult >= 10)
            {
                switch (sdTestResult - 10)
                {
                case 1:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 255, 0));   break;
                case 2:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 165, 0));   break;
                case 3:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 0, 255));     break;
                case 4:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 0, 255));   break;
                case 5:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(128, 128, 128)); break;
                default: sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));     break;
                }
            }
            else
            {
                switch (sdTestResult)
                {
                case 1:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 255, 0));   break;
                case 2:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 255, 255));   break;
                default: sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));     break;
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

            if (sdTestResult == 0)
            {
                uint32_t total_mb = (g_sd_card_info.LogBlockNbr / 2) / 1024;
                uint32_t free_mb  = g_sd_free_kb / 1024;
                uint32_t used_mb  = total_mb - free_mb;

                /* C snprintf (supports %lu) then convert ASCII->Unicode.
                 * TouchGFX Unicode::snprintf needs a UnicodeChar format and
                 * has no %lu support, so this two-step is simpler. */
                char tmp[50];
                snprintf(tmp, sizeof(tmp),
                    "Total:%lu MB  Free:%lu MB  Used:%lu MB",
                    (unsigned long)total_mb, (unsigned long)free_mb,
                    (unsigned long)used_mb);
                touchgfx::Unicode::strncpy(capacityBuf, tmp,
                    sizeof(capacityBuf) / sizeof(capacityBuf[0]));

                textArea1.setWildcard(capacityBuf);
                textArea1.invalidate();

                /* 文件列表暂时禁用（f_opendir 和 defaultTask SD 操作冲突导致卡死）。
                 * 后续改用 defaultTask 列文件到全局变量，SD_Test_Screen 只读显示。*/
                touchgfx::Unicode::strncpy(fileBuf, "File list disabled (SD conflict)",
                    sizeof(fileBuf) / sizeof(fileBuf[0]));
                fileText.setWildcard(fileBuf);
                fileText.invalidate();
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
