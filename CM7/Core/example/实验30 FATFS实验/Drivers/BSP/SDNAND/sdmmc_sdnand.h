/**
 ****************************************************************************************************
 * @file        sdmmc_sdnand.h
 * @version     V1.0
 * @brief       SD NAND 驱动代码
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    STM32H747XIH6小系统板
 *
 ****************************************************************************************************
 */

#ifndef __SDMMC_SDNAND_H
#define __SDMMC_SDNAND_H

#include "./SYSTEM/sys/sys.h"


/* 用户配置区
 * SDMMC时钟计算公式: SDMMC_CK时钟 = sdmmc_ker_ck / [2 * clkdiv]; 其中,sdmmc_ker_ck默认选择来自pll1_q_ck,为240Mhz
 * 注意：SDMMC_INIT_CLK_DIV 在HAL库有定义，我们只要把它的值改为300,即(0x12C),请到stm32h7xx_ll_sdmmc.h修改。
 */
//#define SDMMC_INIT_CLK_DIV        300       /* SDMMC初始化频率,240M / (300 * 2) = 400Khz,最大400Khz */


/******************************************************************************************/
/* SDMMC2的信号线: SD2_D0 ~ SD2_D3/SD2_CLK/SD2_CMD 引脚 定义 
 * 如果你使用了其他引脚做SDMMC2的信号线,修改这里宏定义即可适配.
 * 注意, 这里仅支持SDMMC2, 我们使用的也是SDMMC2. 不支持SDMMMC1.
 */

#define SD2_D0_GPIO_PORT                GPIOB
#define SD2_D0_GPIO_PIN                 GPIO_PIN_14
#define SD2_D0_GPIO_AF                  GPIO_AF9_SDMMC2
#define SD2_D0_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

#define SD2_D1_GPIO_PORT                GPIOB
#define SD2_D1_GPIO_PIN                 GPIO_PIN_15
#define SD2_D1_GPIO_AF                  GPIO_AF9_SDMMC2
#define SD2_D1_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

#define SD2_D2_GPIO_PORT                GPIOB
#define SD2_D2_GPIO_PIN                 GPIO_PIN_3
#define SD2_D2_GPIO_AF                  GPIO_AF9_SDMMC2
#define SD2_D2_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

#define SD2_D3_GPIO_PORT                GPIOB
#define SD2_D3_GPIO_PIN                 GPIO_PIN_4
#define SD2_D3_GPIO_AF                  GPIO_AF9_SDMMC2
#define SD2_D3_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

#define SD2_CLK_GPIO_PORT               GPIOD
#define SD2_CLK_GPIO_PIN                GPIO_PIN_6
#define SD2_CLK_GPIO_AF                 GPIO_AF11_SDMMC2
#define SD2_CLK_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

#define SD2_CMD_GPIO_PORT               GPIOA
#define SD2_CMD_GPIO_PIN                GPIO_PIN_0
#define SD2_CMD_GPIO_AF                 GPIO_AF9_SDMMC2
#define SD2_CMD_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

/******************************************************************************************/

#define SDNAND_TIMEOUT             ((uint32_t)100000000)                              /* 超时时间 */
#define SDNAND_TRANSFER_OK         ((uint8_t)0x00)                                    /* 传输完成 */
#define SDNAND_TRANSFER_BUSY       ((uint8_t)0x01)                                    /* 卡正忙 */

/* 根据 SD_HandleTypeDef 定义的宏，用于快速计算容量 */
#define SD_TOTAL_SIZE_BYTE(__Handle__)  (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>0)
#define SD_TOTAL_SIZE_KB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>10)
#define SD_TOTAL_SIZE_MB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>20)
#define SD_TOTAL_SIZE_GB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>30)

extern SD_HandleTypeDef        g_sdnand_handle;                                       /* SD NAND句柄 */
extern HAL_SD_CardInfoTypeDef  g_sdnand_info_handle;                                  /* SD NAND信息结构体 */

/******************************************************************************************/

uint8_t sdnand_init(void);                                                            /* 初始化SD NAND */
uint8_t get_sdnand_info(HAL_SD_CardInfoTypeDef *cardinfo);                            /* 获取卡信息函数 */
uint8_t get_sdnand_state(void);                                                       /* 获取SD NAND状态 */
uint8_t sdnand_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt);                /* 读SD NAND指定数量块的数据 */
uint8_t sdnand_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt);               /* 写SD NAND指定数量块的数据 */

#endif






