/**
 * TFT SPI 位Bang 驱动 — 用于 NV3052C 等 SPI 面板的初始化寄存器配置
 * ================================================================
 * 初始化:        tft_spi_init()                 仅 0X5571 面板需要, 0X4384 面板无需调用
 * 写寄存器:      tft_spi_write_reg(reg, data)
 * 写命令:        tft_spi_write_cmd(cmd)
 * 写数据:        tft_spi_write_data(data)
 *
 * 引脚复用说明:
 *   TFT_SPI_SCL → PI13 (与 LTDC_VSYNC 共用)
 *   TFT_SPI_SDA → PI12 (与 LTDC_HSYNC 共用)
 *   SPI 初始化时配置为 GPIO 输出, LTDC 初始化时 HAL_LTDC_MspInit 会重新配置为 AF14_LTDC
 */
#ifndef __TFT_SPI_H
#define __TFT_SPI_H

#include "sys_util.h"

/* TFT_SPI pin definitions
 * TFT_SPI_CS  --> PB13 (TP_MISO)
 * TFT_SPI_SCL --> PI13 (LTDC_VSYNC)
 * TFT_SPI_SDA --> PI12 (LTDC_HSYNC)
 * TFT_SPI_RST --> PH5  (LCD_RST)
 */
#define TFT_SPI_CS_GPIO_PORT                GPIOB
#define TFT_SPI_CS_GPIO_PIN                 GPIO_PIN_13
#define TFT_SPI_CS_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define TFT_SPI_SCL_GPIO_PORT               GPIOI
#define TFT_SPI_SCL_GPIO_PIN                GPIO_PIN_13
#define TFT_SPI_SCL_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)

#define TFT_SPI_SDA_GPIO_PORT               GPIOI
#define TFT_SPI_SDA_GPIO_PIN                GPIO_PIN_12
#define TFT_SPI_SDA_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)

#define TFT_SPI_RST_GPIO_PORT               GPIOH
#define TFT_SPI_RST_GPIO_PIN                GPIO_PIN_5
#define TFT_SPI_RST_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOH_CLK_ENABLE(); }while(0)

#define TFT_SPI_CS(x)        do{ x ? \
                                 HAL_GPIO_WritePin(TFT_SPI_CS_GPIO_PORT, TFT_SPI_CS_GPIO_PIN, GPIO_PIN_SET) : \
                                 HAL_GPIO_WritePin(TFT_SPI_CS_GPIO_PORT, TFT_SPI_CS_GPIO_PIN, GPIO_PIN_RESET); \
                             }while(0)

#define TFT_SPI_SCL(x)        do{ x ? \
                                 HAL_GPIO_WritePin(TFT_SPI_SCL_GPIO_PORT, TFT_SPI_SCL_GPIO_PIN, GPIO_PIN_SET) : \
                                 HAL_GPIO_WritePin(TFT_SPI_SCL_GPIO_PORT, TFT_SPI_SCL_GPIO_PIN, GPIO_PIN_RESET); \
                             }while(0)

#define TFT_SPI_SDA(x)        do{ x ? \
                                 HAL_GPIO_WritePin(TFT_SPI_SDA_GPIO_PORT, TFT_SPI_SDA_GPIO_PIN, GPIO_PIN_SET) : \
                                 HAL_GPIO_WritePin(TFT_SPI_SDA_GPIO_PORT, TFT_SPI_SDA_GPIO_PIN, GPIO_PIN_RESET); \
                             }while(0)

#define TFT_SPI_RST(x)       do{ x ? \
                                 HAL_GPIO_WritePin(TFT_SPI_RST_GPIO_PORT, TFT_SPI_RST_GPIO_PIN, GPIO_PIN_SET) : \
                                 HAL_GPIO_WritePin(TFT_SPI_RST_GPIO_PORT, TFT_SPI_RST_GPIO_PIN, GPIO_PIN_RESET); \
                             }while(0)

void tft_spi_init(void);
void tft_spi_write_byte(uint8_t buf);
void tft_spi_write_cmd(uint8_t cmd);
void tft_spi_write_data(uint8_t data);
void tft_spi_write_reg(uint8_t reg, uint8_t data);

#endif
