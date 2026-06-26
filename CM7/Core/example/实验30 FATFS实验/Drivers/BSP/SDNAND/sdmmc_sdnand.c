/**
 ****************************************************************************************************
 * @file        sdmmc_sdnand.c
 * @version     V1.0
 * @brief       SD NAND 驱动代码
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    STM32H747XIH6小系统板
 *
 ****************************************************************************************************
 */

#include "string.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/SDNAND/sdmmc_sdnand.h"


SD_HandleTypeDef g_sdnand_handle;                /* SD NAND句柄 */
HAL_SD_CardInfoTypeDef g_sdnand_info_handle;     /* SD NAND信息结构体 */

/* sdnand_read_disk/sdnand_write_disk函数专用buf,当这两个函数的数据缓存区地址不是4字节对齐的时候,
 * 需要用到该数组,确保数据缓存区地址是4字节对齐的.
 */
//__ALIGNED(4) uint8_t g_sd_data_buffer[512];


/**
 * @brief       初始化SD NAND
 * @param       无
 * @retval      返回值:0,初始化正确;1,初始化错误.
 */
uint8_t sdnand_init(void)
{
    uint8_t SD_Error;
    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_SDMMC2_CLK_ENABLE();                       /* SDMMC2时钟使能 */
    SD2_D0_GPIO_CLK_ENABLE();                            /* SD2_D0引脚IO时钟使能 */
    SD2_D1_GPIO_CLK_ENABLE();                            /* SD2_D1引脚IO时钟使能 */
    SD2_D2_GPIO_CLK_ENABLE();                            /* SD2_D2引脚IO时钟使能 */
    SD2_D3_GPIO_CLK_ENABLE();                            /* SD2_D3引脚IO时钟使能 */
    SD2_CLK_GPIO_CLK_ENABLE();                           /* SD2_CLK引脚IO时钟使能 */
    SD2_CMD_GPIO_CLK_ENABLE();                           /* SD2_CMD引脚IO时钟使能 */      

    gpio_init_struct.Pin = SD2_D0_GPIO_PIN;              /* SD2_D0引脚 */
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;             /* 复用推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                 /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  /* 高速 */
    gpio_init_struct.Alternate = SD2_D0_GPIO_AF;         /* 复用为SDMMC2_D0 */
    HAL_GPIO_Init(SD2_D0_GPIO_PORT, &gpio_init_struct);  /* 初始化SD2_D0引脚 */

    gpio_init_struct.Pin = SD2_D1_GPIO_PIN;              /* SD2_D1引脚 */
    gpio_init_struct.Alternate = SD2_D1_GPIO_AF;         /* 复用为SDMMC2_D1 */
    HAL_GPIO_Init(SD2_D1_GPIO_PORT, &gpio_init_struct);  /* 初始化SD2_D1引脚 */

    gpio_init_struct.Pin = SD2_D2_GPIO_PIN;              /* SD2_D2引脚 */
    gpio_init_struct.Alternate = SD2_D2_GPIO_AF;         /* 复用为SDMMC2_D2 */
    HAL_GPIO_Init(SD2_D2_GPIO_PORT, &gpio_init_struct);  /* 初始化SD2_D2引脚 */

    gpio_init_struct.Pin = SD2_D3_GPIO_PIN;              /* SD2_D3引脚 */
    gpio_init_struct.Alternate = SD2_D3_GPIO_AF;         /* 复用为SDMMC2_D3 */
    HAL_GPIO_Init(SD2_D3_GPIO_PORT, &gpio_init_struct);  /* 初始化SD2_D3引脚 */

    gpio_init_struct.Pin = SD2_CLK_GPIO_PIN;             /* SD2_CLK引脚 */
    gpio_init_struct.Alternate = SD2_CLK_GPIO_AF;        /* 复用为SDMMC2_CLK */
    HAL_GPIO_Init(SD2_CLK_GPIO_PORT, &gpio_init_struct); /* 初始化SD2_CLK引脚 */

    gpio_init_struct.Pin = SD2_CMD_GPIO_PIN;             /* SD2_CMD引脚 */
    gpio_init_struct.Alternate = SD2_CMD_GPIO_AF;        /* 复用为SDMMC2_CMD */
    HAL_GPIO_Init(SD2_CMD_GPIO_PORT, &gpio_init_struct); /* 初始化SD2_CMD引脚 */
        
    HAL_SD_DeInit(&g_sdnand_handle);

    /* 初始化时的时钟不能大于400KHZ */
    g_sdnand_handle.Instance = SDMMC2;
    g_sdnand_handle.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;                       /* 上升沿 */
    g_sdnand_handle.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;           /* 空闲时不关闭时钟电源 */
    g_sdnand_handle.Init.BusWide = SDMMC_BUS_WIDE_4B;                               /* 4位数据线 */
    g_sdnand_handle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE; /* 关闭硬件流控制 */
    g_sdnand_handle.Init.ClockDiv = SDMMC_NSpeed_CLK_DIV;                           /* 设置时钟分频系数，配置为默认SD传输时钟频率 
                                                                                     * SDMMC_CK时钟 = sdmmc_ker_ck / [2 * clkdiv]; (sdmmc_ker_ck一般为240Mhz)
                                                                                     */
  
    SD_Error = HAL_SD_Init(&g_sdnand_handle);
  
    if (SD_Error != HAL_OK)
    {
        return 1;
    }
    
    HAL_SD_GetCardInfo(&g_sdnand_handle, &g_sdnand_info_handle);                    /* 获取SD NAND信息 */

    return 0;
}

/**
 * @brief       获取卡信息函数
 * @param       cardinfo:卡信息存储结构体指针
 * @retval      返回值:读取卡信息状态值
 */
uint8_t get_sdnand_info(HAL_SD_CardInfoTypeDef *cardinfo)
{
    uint8_t sta;
    sta = HAL_SD_GetCardInfo(&g_sdnand_handle, cardinfo);
    return sta;
}

/**
 * @brief       获取SD NAND状态
 * @param       无
 * @retval      返回值:SDNAND_TRANSFER_OK      传输完成，可以继续下一次传输
                       SDNAND_TRANSFER_BUSY    SD NAND正忙，不可以进行下一次传输
 */
uint8_t get_sdnand_state(void)
{
    return ((HAL_SD_GetCardState(&g_sdnand_handle) == HAL_SD_CARD_TRANSFER) ? SDNAND_TRANSFER_OK : SDNAND_TRANSFER_BUSY);
}

/**
 * @brief       读SD NAND指定数量块的数据(fatfs/usb调用)
 * @param       pbuf  : 数据缓存区
 * @param       saddr : 扇区地址
 * @param       cnt   : 扇区个数
 * @retval      0, 正常;  其他, 错误;
 */
uint8_t sdnand_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
    uint8_t sta = HAL_OK;
    uint32_t timeout = SDNAND_TIMEOUT;
    long long lsaddr = saddr;
  
    sys_intx_disable();                                                                        /* 关闭总中断(POLLING模式,严禁中断打断SDMMC读写操作!!!) */
    sta = HAL_SD_ReadBlocks(&g_sdnand_handle, (uint8_t *)pbuf, lsaddr, cnt, SDNAND_TIMEOUT);   /* 多个sector的读操作 */

    while (get_sdnand_state() != SDNAND_TRANSFER_OK)                                           /* 等待SD NAND读完 */
    {
        if (timeout-- == 0)
        {
            sta = SDNAND_TRANSFER_BUSY;
        }
    }

    sys_intx_enable();                                                                         /* 开启总中断 */
    
    return sta;
}

/**
 * @brief       写SD NAND指定数量块的数据(fatfs/usb调用)
 * @param       pbuf  : 数据缓存区
 * @param       saddr : 扇区地址
 * @param       cnt   : 扇区个数
 * @retval      0, 正常;  其他, 错误;
 */
uint8_t sdnand_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
    uint8_t sta = HAL_OK;
    uint32_t timeout = SDNAND_TIMEOUT;
    long long lsaddr = saddr;
  
    sys_intx_disable();                                                                        /* 关闭总中断(POLLING模式,严禁中断打断SDMMC读写操作!!!) */
    sta = HAL_SD_WriteBlocks(&g_sdnand_handle, (uint8_t *)pbuf, lsaddr, cnt, SDNAND_TIMEOUT);  /* 多个sector的写操作 */
           
    while (get_sdnand_state() != SDNAND_TRANSFER_OK)                                           /* 等待SD NAND写完 */
    {
        if (timeout-- == 0)
        {
            sta = SDNAND_TRANSFER_BUSY;
        }
    }

    sys_intx_enable();                                                                         /* 开启总中断 */
    
    return sta;
}






