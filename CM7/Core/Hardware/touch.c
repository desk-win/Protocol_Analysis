/**
 * 触摸屏抽象层 — 仅支持电容触摸
 * ==================================================
 * 使用硬件 I2C2 (PB10/PB11), 由 CubeMX 初始化
 */

#include <stdio.h>
#include "touch.h"
#include "gt9xxx.h"
#include "ft5206.h"
#include "lcd.h"

_m_tp_dev tp_dev = {0};

uint8_t tp_init(void)
{
    tp_dev.touchtype = 0;
    tp_dev.touchtype |= lcddev.dir & 0X01;

    printf("tp_init: lcddev.dir=%d, touchtype=0x%02X\r\n", lcddev.dir, tp_dev.touchtype);

    if (gt9xxx_init() == 0)
    {
        tp_dev.scan = gt9xxx_scan;
        tp_dev.touchtype |= 0X80;
        printf("tp_init: GT9xxx OK (hardware I2C)\r\n");
        return 0;
    }

    if (ft5206_init() == 0)
    {
        tp_dev.scan = ft5206_scan;
        tp_dev.touchtype |= 0X80;
        printf("tp_init: FT5206 OK (hardware I2C)\r\n");
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
