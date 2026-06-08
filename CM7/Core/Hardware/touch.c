/**
 * 触摸屏抽象层 — 仅支持电容触摸
 * ==================================================
 * 轮询 + 节流模式 (每 10 帧读一次 I2C, 有效触摸时立即再读)
 * INT (PB5) 仅 init 期间临时借用, 运行时归还 FMC
 */

#include <stdio.h>
#include "touch.h"
#include "ctiic.h"
#include "gt9xxx.h"
#include "ft5206.h"
#include "lcd.h"
#include "delay.h"

_m_tp_dev tp_dev = {0};

uint8_t tp_init(void)
{
    tp_dev.touchtype = 0;
    tp_dev.touchtype |= lcddev.dir & 0X01;

    printf("tp_init: lcddev.dir=%d, touchtype=0x%02X\r\n", lcddev.dir, tp_dev.touchtype);

    ct_iic_init();
    ct_iic_scan();

    printf("tp_init: trying GT9xxx (I2C addr 0x28)...\r\n");

    if (gt9xxx_init() == 0)
    {
        tp_dev.scan = gt9xxx_scan;
        tp_dev.touchtype |= 0X80;
        printf("tp_init: GT9xxx OK\r\n");
        return 0;
    }

    printf("tp_init: GT9xxx failed, trying FT5206 (I2C addr 0x70)...\r\n");

    if (ft5206_init() == 0)
    {
        tp_dev.scan = ft5206_scan;
        tp_dev.touchtype |= 0X80;
        printf("tp_init: FT5206 OK\r\n");
        return 0;
    }

    printf("tp_init: No touch IC found!\r\n");
    return 1;
}

uint8_t tp_scan(uint8_t mode)
{
    if (tp_dev.scan)
        return tp_dev.scan(mode);
    return 0;
}
