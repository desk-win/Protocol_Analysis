---
name: module-deps
description: 模块依赖关系：关键模块的 #include 链和数据流向
metadata:
  type: project
---

# 模块依赖关系

## CM7 #include 依赖图

```
main.c
├── FreeRTOS.h, cmsis_os2.h        [RTOS]
├── dma.h, dma2d.h                  [DMA/DMA2D]
├── i2c.h                           [触摸面板 I2C]
├── ltdc.h                          [LTDC 控制器 HAL]
├── tim.h                           [TIM3/TIM6]
├── gpio.h                          [GPIO 输出]
├── fmc.h                           [SDRAM FMC]
└── stm32h7xx_hal_rcc.h            [用户自定义]

freertos.c (CM7)
├── FreeRTOS.h, task.h, cmsis_os2.h
├── main.h
└── Hardware: LCD/lcd.h, display.h  [显示应用层]

display.c
├── display.h (DISP_FB_ADDR=0xD0000000)
├── LCD/font.h (字库)
└── 直接操作 DMA2D 寄存器 (R2M Register-to-Memory mode)
```

## CM4 #include 依赖图

```
main.c
├── FreeRTOS.h, cmsis_os2.h
├── dcmi.h (DCMI camera interface)
└── dma.h (DMA1_Stream0 for DCMI)

freertos.c (CM4)
├── FreeRTOS.h, task.h, cmsis_os2.h
└── main.h (仅空壳 defaultTask)
```

## 数据流向

```
[DCMI 摄像头] → DMA1_Stream0(Circular) → 内存缓冲区 (CM4)
                                              ↓ (待实现核间数据传递)
[FMC SDRAM 0xD0000000 (帧缓冲)] ← DMA2D(填充) ← CPU(画点/文字)
                                              ↓
[LTDC 控制器] → LCD RGB888 Panel (800x480)
```

## 模块层级

| 层级 | 模块 | 文件位置 |
|------|------|---------|
| **应用层** | 显示测试, defaultTask | [freertos.c](CM7/Core/Src/freertos.c), [freertos.c](CM4/Core/Src/freertos.c) |
| **硬件抽象层** | display, lcd, ltdc | [CM7/Core/Hardware/](CM7/Core/Hardware/) |
| **HAL 层** | FMC, LTDC, DMA2D, I2C2, DCMI, DMA, GPIO, TIM | [CM7/Core/Src/](CM7/Core/Src/), [CM4/Core/Src/](CM4/Core/Src/) |
| **驱动层** | STM32H7xx HAL | [Drivers/STM32H7xx_HAL_Driver/](Drivers/STM32H7xx_HAL_Driver/) |
| **中间件** | FreeRTOS CMSIS_V2 | [Middlewares/Third_Party/FreeRTOS/](Middlewares/Third_Party/FreeRTOS/) |
| **启动层** | CMSIS, 双核引导 | [Drivers/CMSIS/](Drivers/CMSIS/), [Common/](Common/) |
