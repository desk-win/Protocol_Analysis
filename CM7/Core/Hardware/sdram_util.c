#include "sdram_util.h"

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

    if (HAL_SDRAM_SendCommand(&hsdram2, &command, 0X1000) == HAL_OK)
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
