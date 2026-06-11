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
 * @brief  FatFs test: mount + read root directory
 * @retval 0=success, 1=mount fail, 2=opendir fail
 */
static int fatfs_test(void)
{
    FRESULT fr;
    fr = f_mount(&SDFatFS, SDPath, 1);
    if (fr != FR_OK) return 1;

    DIR dir;
    fr = f_opendir(&dir, "/");
    if (fr != FR_OK) return 2;

    FILINFO fno;
    f_readdir(&dir, &fno);
    f_closedir(&dir);
    return 0;
}

screenView::screenView()
    : touchDotAdded(false), sdTestDone(false), sdTestResult(-1),
      fatfsResult(-1), tickCount(0), sdStatusBarAdded(false)
{
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

    sdStatusBar.setPosition(50, 14, 200, 8);
    sdStatusBar.setVisible(false);
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
                fatfsResult = fatfs_test();
                sdTestResult = (fatfsResult == 0) ? 0 : 10 + fatfsResult;
            }
            else
            {
                sdTestResult = det;
            }

            /* Color = diagnostic */
            if (sdTestResult == 0)
                sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 255, 0));         /* GREEN  = all ok */
            else if (sdTestResult >= 10)
                sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 165, 0));       /* ORANGE = SD ok, FatFs fail */
            else
            {
                switch (sdTestResult)
                {
                case 1:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 255, 0));   break; /* YELLOW  = HAL_ERROR */
                case 2:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 255, 255));   break; /* CYAN    = HAL_TIMEOUT */
                case 3:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(0, 0, 255));     break; /* BLUE    = HAL_BUSY */
                case 4:  sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(128, 0, 128));   break; /* PURPLE  = other HAL status */
                default: sdStatusBar.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0));     break; /* RED */
                }
            }
            sdStatusBar.invalidate();

            if (!touchDotAdded) { add(touchDot); touchDotAdded = true; }
            touchDotPainter.setColor(
                (sdTestResult == 0) ? touchgfx::Color::getColorFromRGB(0, 255, 0)
                                    : touchgfx::Color::getColorFromRGB(255, 0, 0));
            touchDot.setPainter(touchDotPainter);
            touchDot.setRadius(15);
            touchDot.setCenter(16, 16);
            touchDot.setPosition(10, 10, 32, 32);
            touchDot.setVisible(true);
            touchDot.invalidate();
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
        touchDot.setRadius(15);
        touchDot.setCenter(16, 16);
        touchDot.setPosition(10, 10, 32, 32);
        touchDot.setVisible(true);
        touchDot.invalidate();
    }
}
