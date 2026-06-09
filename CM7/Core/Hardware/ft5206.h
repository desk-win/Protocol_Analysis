/**
 * 触摸屏驱动 — FT5206 / FT5426 电容触摸 IC
 * ==================================================
 * 引脚映射:
 *   RST = PB12  — 推挽输出
 *   INT = PB5   — GPIO_Input (CubeMX 配置)
 *   I2C = 硬件 I2C2 (PB10/PB11)
 */
#ifndef __FT5206_H
#define __FT5206_H

#include "main.h"

/* ── RST 引脚: PB12 ─────────────────────────────────────────────────── */
#define FT5206_RST_GPIO_PORT        GPIOB
#define FT5206_RST_GPIO_PIN         GPIO_PIN_12
#define FT5206_RST_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOB_CLK_ENABLE(); } while(0)

#define FT5206_RST(x)   do { x ? \
    HAL_GPIO_WritePin(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN, GPIO_PIN_SET) : \
    HAL_GPIO_WritePin(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN, GPIO_PIN_RESET); \
} while(0)

/* ── INT 引脚: PB5 (与 FMC_SDCKE1 共用, 仅 init 期间临时借用) ──────── */
#define FT5206_INT_GPIO_PORT        GPIOB
#define FT5206_INT_GPIO_PIN         GPIO_PIN_5
#define FT5206_INT_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOB_CLK_ENABLE(); } while(0)

/* ── I2C 设备地址 ─────────────────────────────────────────────────────── */
#define FT5206_CMD_WR       0X70
#define FT5206_CMD_RD       0X71

/* ── FT5206 寄存器地址 ────────────────────────────────────────────────── */
#define FT5206_DEVIDE_MODE      0x00
#define FT5206_REG_NUM_FINGER   0x02
#define FT5206_TP1_REG          0X03
#define FT5206_TP2_REG          0X09
#define FT5206_TP3_REG          0X0F
#define FT5206_TP4_REG          0X15
#define FT5206_TP5_REG          0X1B
#define FT5206_ID_G_LIB_VERSION 0xA1
#define FT5206_ID_G_MODE        0xA4
#define FT5206_ID_G_THGROUP     0x80
#define FT5206_ID_G_PERIODACTIVE 0x88

/* ── 函数声明 ─────────────────────────────────────────────────────────── */
uint8_t ft5206_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len);
void    ft5206_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len);
uint8_t ft5206_init(void);
uint8_t ft5206_scan(uint8_t mode);

#endif /* __FT5206_H */
