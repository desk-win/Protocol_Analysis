/**
 * 触摸控制器 I2C 通信 — GPIO 模拟 I2C (软件 I2C)
 * ==================================================
 * 引脚映射 (实际硬件):
 *   SCL = PG7  — 推挽输出 + 上拉
 *   SDA = PG12  — 开漏输出 + 上拉
 *
 * 
 *       
 */
#ifndef __CTIIC_H
#define __CTIIC_H

#include "main.h"

/* ── I2C 引脚 — 与例程一致: PB10(SCL) / PB11(SDA) ─────────────────── */
#define CT_IIC_SCL_GPIO_PORT            GPIOB
#define CT_IIC_SCL_GPIO_PIN         GPIO_PIN_10
#define CT_IIC_SCL_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOB_CLK_ENABLE(); } while(0)

#define CT_IIC_SDA_GPIO_PORT            GPIOB
#define CT_IIC_SDA_GPIO_PIN         GPIO_PIN_11
#define CT_IIC_SDA_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOB_CLK_ENABLE(); } while(0)

/* ── IO 操作宏 ────────────────────────────────────────────────────────── */
#define CT_IIC_SCL(x)   do { x ? \
    HAL_GPIO_WritePin(CT_IIC_SCL_GPIO_PORT, CT_IIC_SCL_GPIO_PIN, GPIO_PIN_SET) : \
    HAL_GPIO_WritePin(CT_IIC_SCL_GPIO_PORT, CT_IIC_SCL_GPIO_PIN, GPIO_PIN_RESET); \
} while(0)

#define CT_IIC_SDA(x)   do { x ? \
    HAL_GPIO_WritePin(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN, GPIO_PIN_SET) : \
    HAL_GPIO_WritePin(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN, GPIO_PIN_RESET); \
} while(0)

#define CT_READ_SDA     HAL_GPIO_ReadPin(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN)

/* ── 函数声明 ─────────────────────────────────────────────────────────── */
void     ct_iic_init(void);
void     ct_iic_start(void);
void     ct_iic_stop(void);
void     ct_iic_ack(void);
void     ct_iic_nack(void);
uint8_t  ct_iic_wait_ack(void);
void     ct_iic_send_byte(uint8_t txd);
uint8_t  ct_iic_read_byte(uint8_t ack);
void     ct_iic_scan(void);              /* Scan bus, print found addresses via printf */

#endif /* __CTIIC_H */
