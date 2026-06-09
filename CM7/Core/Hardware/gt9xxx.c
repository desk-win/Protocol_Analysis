/**
 * 触摸屏驱动 — GT9xxx 电容触摸 IC 实现
 * ==================================================
 * 引脚: RST=PB12, INT=PB5, I2C=PB10/PB11 (硬件 I2C2)
 * PB5 已从 FMC 释放（CubeMX 配为 GPIO_Input）
 */

#include <string.h>
#include <stdio.h>
#include "gt9xxx.h"
#include "i2c.h"
#include "touch.h"
#include "lcd.h"
#include "delay.h"

static uint8_t g_gt_tnum = 5;

/* ── I2C read / write (hardware I2C2) ────────────────────────────────── */

#define GT9XXX_I2C_TIMEOUT  100     /* ms */

uint8_t gt9xxx_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Write(&hi2c2, GT9XXX_CMD_WR, reg,
                               I2C_MEMADD_SIZE_16BIT, buf, len,
                               GT9XXX_I2C_TIMEOUT);
    return (status != HAL_OK) ? 1 : 0;
}

void gt9xxx_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    HAL_I2C_Mem_Read(&hi2c2, GT9XXX_CMD_WR, reg,
                     I2C_MEMADD_SIZE_16BIT, buf, len,
                     GT9XXX_I2C_TIMEOUT);
}

/* ── Init ────────────────────────────────────────────────────────────── */

uint8_t gt9xxx_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    uint8_t temp[5];

    GT9XXX_RST_GPIO_CLK_ENABLE();
    GT9XXX_INT_GPIO_CLK_ENABLE();

    /* RST: 推挽输出 */
    gpio_init_struct.Pin   = GT9XXX_RST_GPIO_PIN;
    gpio_init_struct.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull  = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GT9XXX_RST_GPIO_PORT, &gpio_init_struct);

    /* INT (PB5): 输入上拉 — GT9xxx 在 RST 上升沿采样 INT: HIGH → addr 0x28/0x29 */
    gpio_init_struct.Pin   = GT9XXX_INT_GPIO_PIN;
    gpio_init_struct.Mode  = GPIO_MODE_INPUT;
    gpio_init_struct.Pull  = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio_init_struct);

    /* 硬件复位序列 */
    GT9XXX_RST(0);
    delay_ms(30);
    GT9XXX_RST(1);
    delay_ms(30);

    /* 复位完成, INT 改为浮空 (GT9xxx 要求) */
    gpio_init_struct.Pin   = GT9XXX_INT_GPIO_PIN;
    gpio_init_struct.Mode  = GPIO_MODE_INPUT;
    gpio_init_struct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio_init_struct);

    delay_ms(100);

    /* 读取产品 ID */
    gt9xxx_rd_reg(GT9XXX_PID_REG, temp, 4);
    temp[4] = 0;
    printf("CTP ID:%s\r\n", temp);

    if (strcmp((char *)temp, "911") && strcmp((char *)temp, "9147") &&
        strcmp((char *)temp, "1158") && strcmp((char *)temp, "9271") &&
        strcmp((char *)temp, "967"))
    {
        /* 识别失败 */
        return 1;
    }

    if (strcmp((char *)temp, "9271") == 0)
        g_gt_tnum = 10;

    /* 软复位 */
    temp[0] = 0X02;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);
    delay_ms(10);
    temp[0] = 0X00;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);

    return 0;
}

/* ── Scan (polling with throttle) ────────────────────────────────────── */

static const uint16_t GT9XXX_TPX_TBL[10] = {
    GT9XXX_TP1_REG, GT9XXX_TP2_REG, GT9XXX_TP3_REG, GT9XXX_TP4_REG, GT9XXX_TP5_REG,
    GT9XXX_TP6_REG, GT9XXX_TP7_REG, GT9XXX_TP8_REG, GT9XXX_TP9_REG, GT9XXX_TP10_REG,
};

/**
 * @brief  扫描触摸 (轮询模式)
 * @note   使用 t%10 节流:
 *         - 首次 10 帧每帧读 I2C (快速检测首次触摸)
 *         - 之后每 10 帧读一次 I2C (降低总线占用)
 *         - 有效触摸后 t=0 立即重置, 保证拖拽响应
 *         - 非读取帧 tp_dev.sta 保持上次值, 不清零
 * @param  mode: 未使用 (保留接口兼容)
 * @retval 1=有触摸, 0=无触摸
 */
uint8_t gt9xxx_scan(uint8_t mode)
{
    uint8_t buf[4];
    uint8_t i = 0;
    uint8_t res = 0;
    uint16_t temp;
    uint16_t tempsta;
    static uint8_t t = 0;

    t++;

    if ((t % 10) == 0 || t < 10)
    {
        gt9xxx_rd_reg(GT9XXX_GSTID_REG, &mode, 1);

        if ((mode & 0X80) && ((mode & 0XF) <= g_gt_tnum))
        {
            i = 0;
            gt9xxx_wr_reg(GT9XXX_GSTID_REG, &i, 1);
        }

        if ((mode & 0XF) && ((mode & 0XF) <= g_gt_tnum))
        {
            temp = 0XFFFF << (mode & 0XF);
            tempsta = tp_dev.sta;
            tp_dev.sta = (~temp) | TP_PRES_DOWN | TP_CATH_PRES;
            tp_dev.x[g_gt_tnum - 1] = tp_dev.x[0];
            tp_dev.y[g_gt_tnum - 1] = tp_dev.y[0];

            for (i = 0; i < g_gt_tnum; i++)
            {
                if (tp_dev.sta & (1 << i))
                {
                    gt9xxx_rd_reg(GT9XXX_TPX_TBL[i], buf, 4);

                    /* RGB 接口屏坐标映射 */
                    if (tp_dev.touchtype & 0X01)        /* 横屏 */
                    {
                        tp_dev.x[i] = ((uint16_t)buf[1] << 8) + buf[0];
                        tp_dev.y[i] = ((uint16_t)buf[3] << 8) + buf[2];
                    }
                    else                                /* 竖屏 */
                    {
                        tp_dev.x[i] = lcddev.width - (((uint16_t)buf[3] << 8) + buf[2]);
                        tp_dev.y[i] = ((uint16_t)buf[1] << 8) + buf[0];
                    }
                }
            }

            res = 1;

            if (tp_dev.x[0] > lcddev.width || tp_dev.y[0] > lcddev.height)
            {
                if ((mode & 0XF) > 1)
                {
                    tp_dev.x[0] = tp_dev.x[1];
                    tp_dev.y[0] = tp_dev.y[1];
                    t = 0;          /* 多指时立即重读 */
                }
                else
                {
                    tp_dev.x[0] = tp_dev.x[g_gt_tnum - 1];
                    tp_dev.y[0] = tp_dev.y[g_gt_tnum - 1];
                    mode = 0X80;
                    tp_dev.sta = tempsta;
                }
            }
            else
            {
                t = 0;              /* 有效触摸, 立即重置以加快下次读取 */
            }
        }

        if ((mode & 0X8F) == 0X80)
        {
            if (tp_dev.sta & TP_PRES_DOWN)
            {
                tp_dev.sta &= ~TP_PRES_DOWN;
            }
            else
            {
                tp_dev.x[0] = 0xffff;
                tp_dev.y[0] = 0xffff;
                tp_dev.sta &= 0XE000;
            }
        }
    }

    if (t > 240) t = 10;           /* 防止溢出 */

    return res;
}
