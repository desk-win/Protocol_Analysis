/**
 * 触摸屏抽象层 — 仅支持电容触摸
 * ==================================================
 * 轮询 + 节流模式: scan 函数内部每 10 帧读一次 I2C
 * INT (PB5) 仅 init 期间临时借用, 运行时归还 FMC
 */
#ifndef __TOUCH_H
#define __TOUCH_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TP_PRES_DOWN    0x8000
#define TP_CATH_PRES    0x4000
#define CT_MAX_TOUCH    10

typedef struct
{
    uint8_t  (*scan)(uint8_t);
    uint16_t x[CT_MAX_TOUCH];
    uint16_t y[CT_MAX_TOUCH];
    uint16_t sta;
    uint8_t  touchtype;
} _m_tp_dev;

extern _m_tp_dev tp_dev;

uint8_t tp_init(void);
uint8_t tp_scan(uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif /* __TOUCH_H */
