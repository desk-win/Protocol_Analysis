/**
 * 触摸控制器 I2C 通信 — GPIO 模拟 I2C 实现
 * ==================================================
 * 移植自 正点原子 实验24 ctiic.c, 适配 CubeMX 引脚配置
 */

#include "ctiic.h"
#include "delay.h"
#include "i2c.h"
#include <stdio.h>

/**
 * @brief  I2C 时序延时 (约 2 us)
 */
static void ct_iic_delay(void)
{
    delay_us(2);
}

/**
 * @brief  触摸 I2C 接口初始化
 * @note   PG7(SCL)/PG12(SDA) 是普通 GPIO, 用于软件 I2C
 *         
 */
void ct_iic_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    /* 释放硬件 I2C2, 让 PB10/PB11 回到普通 GPIO 模式 */
    HAL_I2C_DeInit(&hi2c2);

    CT_IIC_SCL_GPIO_CLK_ENABLE();
    CT_IIC_SDA_GPIO_CLK_ENABLE();

    /* SCL (PB10): 推挽输出 */
    gpio_init_struct.Pin   = CT_IIC_SCL_GPIO_PIN;
    gpio_init_struct.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull  = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(CT_IIC_SCL_GPIO_PORT, &gpio_init_struct);

    /* SDA (PB11): 开漏输出 */
    gpio_init_struct.Pin   = CT_IIC_SDA_GPIO_PIN;
    gpio_init_struct.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio_init_struct.Pull  = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(CT_IIC_SDA_GPIO_PORT, &gpio_init_struct);

    ct_iic_stop();
}

/**
 * @brief  发送 I2C 开始信号
 */
void ct_iic_start(void)
{
    CT_IIC_SDA(1);
    CT_IIC_SCL(1);
    ct_iic_delay();
    CT_IIC_SDA(0);      /* SCL 高时 SDA 下降沿 → START */
    ct_iic_delay();
    CT_IIC_SCL(0);      /* 钳住 SCL, 准备发送数据 */
    ct_iic_delay();
}

/**
 * @brief  发送 I2C 停止信号
 */
void ct_iic_stop(void)
{
    CT_IIC_SDA(0);
    ct_iic_delay();
    CT_IIC_SCL(1);
    ct_iic_delay();
    CT_IIC_SDA(1);      /* SCL 高时 SDA 上升沿 → STOP */
    ct_iic_delay();
}

/**
 * @brief  等待从机 ACK 应答
 * @retval 0: 收到 ACK; 1: 无应答 (超时)
 */
uint8_t ct_iic_wait_ack(void)
{
    uint8_t waittime = 0;
    uint8_t rack = 0;

    CT_IIC_SDA(1);      /* 释放 SDA (开漏模式, 从机可拉低) */
    ct_iic_delay();
    CT_IIC_SCL(1);      /* SCL=1, 从机此时发送 ACK */
    ct_iic_delay();

    while (CT_READ_SDA)
    {
        waittime++;
        if (waittime > 250)
        {
            ct_iic_stop();
            rack = 1;
            break;
        }
        ct_iic_delay();
    }

    CT_IIC_SCL(0);
    ct_iic_delay();
    return rack;
}

/**
 * @brief  发送 ACK 应答
 */
void ct_iic_ack(void)
{
    CT_IIC_SDA(0);
    ct_iic_delay();
    CT_IIC_SCL(1);
    ct_iic_delay();
    CT_IIC_SCL(0);
    ct_iic_delay();
    CT_IIC_SDA(1);
    ct_iic_delay();
}

/**
 * @brief  发送 NACK 应答
 */
void ct_iic_nack(void)
{
    CT_IIC_SDA(1);
    ct_iic_delay();
    CT_IIC_SCL(1);
    ct_iic_delay();
    CT_IIC_SCL(0);
    ct_iic_delay();
}

/**
 * @brief  I2C 发送一个字节 (MSB first)
 */
void ct_iic_send_byte(uint8_t data)
{
    uint8_t t;

    for (t = 0; t < 8; t++)
    {
        CT_IIC_SDA((data & 0x80) >> 7);
        ct_iic_delay();
        CT_IIC_SCL(1);
        ct_iic_delay();
        CT_IIC_SCL(0);
        data <<= 1;
    }

    CT_IIC_SDA(1);      /* 发送完毕, 释放 SDA */
}

/**
 * @brief  I2C 读取一个字节
 * @param  ack: 1→发送 ACK; 0→发送 NACK
 * @retval 读取到的数据
 */
uint8_t ct_iic_read_byte(uint8_t ack)
{
    uint8_t i, receive = 0;

    for (i = 0; i < 8; i++)
    {
        receive <<= 1;
        CT_IIC_SCL(1);
        ct_iic_delay();
        if (CT_READ_SDA)
            receive++;
        CT_IIC_SCL(0);
        ct_iic_delay();
    }

    if (!ack)
        ct_iic_nack();
    else
        ct_iic_ack();

    return receive;
}

/**
 * @brief  I2C 总线扫描 — 尝试所有 7-bit 地址, 打印有 ACK 的设备
 * @note   串口输出格式: "I2C scan: found 0xXX at addr 0xYY"
 */
void ct_iic_scan(void)
{
    uint8_t addr;
    uint8_t found = 0;

    printf("I2C scan on PG7(SCL)/PG12(SDA) ...\r\n");

    /* 先验证 GPIO 可读写: SCL/SDA 拉高后应读回 1 */
    CT_IIC_SCL(1); CT_IIC_SDA(1);
    ct_iic_delay();
    printf("  SCL=%d, SDA=%d (expect 1,1)\r\n",
           HAL_GPIO_ReadPin(CT_IIC_SCL_GPIO_PORT, CT_IIC_SCL_GPIO_PIN),
           CT_READ_SDA);

    for (addr = 0; addr < 128; addr++)
    {
        ct_iic_start();
        ct_iic_send_byte(addr << 1);        /* 写地址 */
        if (ct_iic_wait_ack() == 0)         /* 收到 ACK */
        {
            printf("  ** Found device at 0x%02X (wr:0x%02X rd:0x%02X)\r\n",
                   addr, (addr << 1), (addr << 1) | 1);
            found++;
        }
        ct_iic_stop();
    }

    if (found == 0)
        printf("  No I2C device found!\r\n");
    else
        printf("  Total: %d device(s)\r\n", found);
}
