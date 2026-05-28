#include "sdram.h"
#include "delay.h"

SDRAM_HandleTypeDef g_sdram_handle;

void sdram_init(void)
{
    FMC_SDRAM_TimingTypeDef sdram_timing;

    g_sdram_handle.Instance = FMC_SDRAM_DEVICE;
    g_sdram_handle.Init.SDBank = FMC_SDRAM_BANK2;
    g_sdram_handle.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_9;
    g_sdram_handle.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
    g_sdram_handle.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_32;
    g_sdram_handle.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
    g_sdram_handle.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_2;
    g_sdram_handle.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    g_sdram_handle.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
    g_sdram_handle.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
    g_sdram_handle.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;

    sdram_timing.LoadToActiveDelay = 2;
    sdram_timing.ExitSelfRefreshDelay = 8;
    sdram_timing.SelfRefreshTime = 6;
    sdram_timing.RowCycleDelay = 8;
    sdram_timing.WriteRecoveryTime = 2;
    sdram_timing.RPDelay = 3;
    sdram_timing.RCDDelay = 3;
    HAL_SDRAM_Init(&g_sdram_handle, &sdram_timing);

    sdram_initialization_sequence();

    /* Refresh count = 64ms * 110MHz / 4096 - 20 = 1699 */
    HAL_SDRAM_ProgramRefreshRate(&g_sdram_handle, 1699);
}

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

uint8_t sdram_send_cmd(uint8_t bankx, uint8_t cmd, uint8_t refresh, uint16_t regval)
{
    uint32_t target_bank = 0;
    FMC_SDRAM_CommandTypeDef command;

    if (bankx == 0)
        target_bank = FMC_SDRAM_CMD_TARGET_BANK1;
    else if (bankx == 1)
        target_bank = FMC_SDRAM_CMD_TARGET_BANK2;

    command.CommandMode = cmd;
    command.CommandTarget = target_bank;
    command.AutoRefreshNumber = refresh;
    command.ModeRegisterDefinition = regval;

    if (HAL_SDRAM_SendCommand(&g_sdram_handle, &command, 0X1000) == HAL_OK)
        return 0;
    else
        return 1;
}

void fmc_sdram_write_buffer(uint8_t *pbuf, uint32_t writeaddr, uint32_t n)
{
    for (; n != 0; n--)
    {
        *(volatile uint8_t *)(BANK6_SDRAM_ADDR + writeaddr) = *pbuf;
        writeaddr++;
        pbuf++;
    }
}

void fmc_sdram_read_buffer(uint8_t *pbuf, uint32_t readaddr, uint32_t n)
{
    for (; n != 0; n--)
    {
        *pbuf++ = *(volatile uint8_t *)(BANK6_SDRAM_ADDR + readaddr);
        readaddr++;
    }
}
