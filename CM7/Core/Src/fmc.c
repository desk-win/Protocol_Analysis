#include "fmc.h"

/* USER CODE BEGIN 0 */
#include "sdram_util.h"
#include "delay.h"
/* USER CODE END 0 */

SDRAM_HandleTypeDef hsdram2;

/* USER CODE BEGIN 1 */

void sdram_initialization_sequence(void)
{
    uint32_t temp = 0;

    sdram_send_cmd(1, FMC_SDRAM_CMD_CLK_ENABLE, 1, 0);
    delay_us(500);
    sdram_send_cmd(1, FMC_SDRAM_CMD_PALL, 1, 0);
    sdram_send_cmd(1, FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8, 0);

    temp = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |
              SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
              SDRAM_MODEREG_CAS_LATENCY_2 |
              SDRAM_MODEREG_OPERATING_MODE_STANDARD |
              SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
    sdram_send_cmd(1, FMC_SDRAM_CMD_LOAD_MODE, 1, temp);
}

/* Compatibility wrapper — called from main() */
void sdram_init(void)
{
    MX_FMC_Init();
}

/* USER CODE END 1 */

void MX_FMC_Init(void)
{
    FMC_SDRAM_TimingTypeDef sdram_timing;

    hsdram2.Instance = FMC_SDRAM_DEVICE;
    hsdram2.Init.SDBank = FMC_SDRAM_BANK2;
    hsdram2.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_9;
    hsdram2.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
    hsdram2.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_32;
    hsdram2.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
    hsdram2.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_2;
    hsdram2.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    hsdram2.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
    hsdram2.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
    hsdram2.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;

    sdram_timing.LoadToActiveDelay = 2;
    sdram_timing.ExitSelfRefreshDelay = 8;
    sdram_timing.SelfRefreshTime = 6;
    sdram_timing.RowCycleDelay = 8;
    sdram_timing.WriteRecoveryTime = 2;
    sdram_timing.RPDelay = 3;
    sdram_timing.RCDDelay = 3;
    HAL_SDRAM_Init(&hsdram2, &sdram_timing);

    sdram_initialization_sequence();

    /* Refresh count = 64ms * 110MHz / 4096 - 20 = 1699 */
    HAL_SDRAM_ProgramRefreshRate(&hsdram2, 1699);
}

void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram)
{
    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    gpio_init_struct.Pin = GPIO_PIN_0;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init_struct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOC, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOH, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOI, &gpio_init_struct);
}

void HAL_SDRAM_MspDeInit(SDRAM_HandleTypeDef* sdramHandle)
{
    if(sdramHandle->Instance==FMC_SDRAM_DEVICE)
    {
        __HAL_RCC_FMC_CLK_DISABLE();

        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15);
        HAL_GPIO_DeInit(GPIOE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
        HAL_GPIO_DeInit(GPIOF, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
        HAL_GPIO_DeInit(GPIOG, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15);
        HAL_GPIO_DeInit(GPIOH, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
        HAL_GPIO_DeInit(GPIOI, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_9 | GPIO_PIN_10);
    }
}
