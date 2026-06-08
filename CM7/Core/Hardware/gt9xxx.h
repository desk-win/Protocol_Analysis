/**
 * 触摸屏驱动 — GT9xxx 电容触摸 IC
 * ==================================================
 * 支持 GT911 / GT9147 / GT1151 / GT9271 / GT967 等
 * 引脚映射 (与正点原子例程一致):
 *   RST = PB12  — 推挽输出
 *   INT = PB5   — 复位时临时 GPIO, 完成后归还 FMC
 *   I2C = 软件 I2C (ctiic.c, PB10/PB11)
 */
#ifndef __GT9XXX_H
#define __GT9XXX_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── RST 引脚: PB12 ─────────────────────────────────────────────────── */
#define GT9XXX_RST_GPIO_PORT        GPIOB
#define GT9XXX_RST_GPIO_PIN         GPIO_PIN_12
#define GT9XXX_RST_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOB_CLK_ENABLE(); } while(0)

#define GT9XXX_RST(x)   do { x ? \
    HAL_GPIO_WritePin(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN, GPIO_PIN_SET) : \
    HAL_GPIO_WritePin(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN, GPIO_PIN_RESET); \
} while(0)

/* ── INT 引脚: PB5 (CubeMX 已配为 GPIO_Input) ──────────────────────── */
#define GT9XXX_INT_GPIO_PORT        GPIOB
#define GT9XXX_INT_GPIO_PIN         GPIO_PIN_5
#define GT9XXX_INT_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOB_CLK_ENABLE(); } while(0)

/* ── I2C 设备地址 ─────────────────────────────────────────────────────── */
#define GT9XXX_CMD_WR       0X28
#define GT9XXX_CMD_RD       0X29

/* ── GT9xxx 寄存器地址 ────────────────────────────────────────────────── */
#define GT9XXX_CTRL_REG     0X8040
#define GT9XXX_CFGS_REG     0X8047
#define GT9XXX_CHECK_REG    0X80FF
#define GT9XXX_PID_REG      0X8140
#define GT9XXX_GSTID_REG    0X814E
#define GT9XXX_TP1_REG      0X8150
#define GT9XXX_TP2_REG      0X8158
#define GT9XXX_TP3_REG      0X8160
#define GT9XXX_TP4_REG      0X8168
#define GT9XXX_TP5_REG      0X8170
#define GT9XXX_TP6_REG      0X8178
#define GT9XXX_TP7_REG      0X8180
#define GT9XXX_TP8_REG      0X8188
#define GT9XXX_TP9_REG      0X8190
#define GT9XXX_TP10_REG     0X8198

/* ── 函数声明 ─────────────────────────────────────────────────────────── */
uint8_t gt9xxx_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len);
void    gt9xxx_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len);
uint8_t gt9xxx_init(void);
uint8_t gt9xxx_scan(uint8_t mode);


#ifdef __cplusplus
}
#endif

#endif /* __GT9XXX_H */
