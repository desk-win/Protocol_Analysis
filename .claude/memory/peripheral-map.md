---
name: peripheral-map
description: 外设分配表：每个外设由哪个核管理，初始化代码位置
metadata:
  type: project
---

# 外设分配表

## Cortex-M7 外设

| 外设 | 初始化文件 | 功能 | 关键参数 |
|------|-----------|------|---------|
| FMC | [cm7 fmc.c](CM7/Core/Src/fmc.c) | SDRAM 32-bit, Bank2 | CL=3, 12-row, 9-col, 4-bank, FMC_CLK=240MHz |
| LTDC | [cm7 ltdc.c](CM7/Core/Src/ltdc.c) | LCD RGB888 800x480 | HBP=8, HFP=8, HSync=4, VBP=8, VFP=8, VSync=4 |
| DMA2D | [cm7 dma2d.c](CM7/Core/Src/dma2d.c) | 硬件图形加速 | RGB888 output |
| I2C2 | [cm7 i2c.c](CM7/Core/Src/i2c.c) | 触摸面板通信 | Timing=0x307075B1 |
| TIM3 | [cm7 tim.c](CM7/Core/Src/tim.c) | 自定义定时器 | Period=11999 |
| TIM6 | — (HAL内部) | FreeRTOS 时基 | CM7 SysTick 替代 |
| DMA | [cm7 dma.c](CM7/Core/Src/dma.c) | 通用 DMA 配置 | — |
| GPIO | [cm7 gpio.c](CM7/Core/Src/gpio.c) | 多路 GPIO 输出 | BL_CTR(PB0), LCD_CS(PD5), LCD_SDA(PD3), LCD_SCL(PD4), LCD_RST(PH5), LCD_PWREN(PI11), CTP_RST(PG11), CTP_SCL(PG7), CTP_SDA(PG12), CTP_INT(PG13) |

## Cortex-M4 外设

| 外设 | 初始化文件 | 功能 | 关键参数 |
|------|-----------|------|---------|
| DCMI | [cm4 dcmi.c](CM4/Core/Src/dcmi.c) | 8-bit 摄像头并行接口 | Hardware Sync, PCK Falling Edge, DMA1_Stream0 Circular |
| DMA | [cm4 dma.c](CM4/Core/Src/dma.c) | DCMI DMA 通道 | 外设→内存, 字宽, 循环模式 |
| GPIO | [cm4 gpio.c](CM4/Core/Src/gpio.c) | 摄像头 GPIO | D0-D7, HSYNC, VSYNC, PIXCLK |

## 共享/双域外设

| 外设 | 说明 |
|------|------|
| RCC | 两个核都可访问，由 CM7 初始化 |
| PWR | 电源管理，CM7 初始化配置 |
| HSEM | Hardware Semaphore 0，用于双核同步 |
| VREFBUF | 电压参考 |
| WWDG1/WWDG2 | 双窗口看门狗 |
| IWDG1/IWDG2 | 双独立看门狗 |
