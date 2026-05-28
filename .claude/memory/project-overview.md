---
name: project-overview
description: qiansai 项目概要：芯片型号、双核架构、RTOS、构建系统
metadata:
  type: project
---

# 项目概览

## 芯片
- **MCU**: STM32H747XIHx (TFBGA240)
- **系列**: STM32H7
- **内核**: 双核 Cortex-M7 + Cortex-M4
- **M7 主频**: 480MHz
- **M4 主频**: 240MHz
- **固件包**: STM32Cube FW_H7 V1.13.0
- **CubeMX 版本**: 6.17.0

## 双核架构
- **Context 0**: Cortex-M7 (主核，负责显示和人机交互)
- **Context 1**: Cortex-M4 (从核，负责摄像头 DCMI)

## 启动流程
- CM7 先启动，初始化系统时钟和所有外设后通过 HSEM 释放 CM4
- CM4 启动前处于 STOP 模式，收到 HSEM 通知后唤醒
- 参见 [[dual-core-arch]]

## RTOS
- 两个核都运行 **FreeRTOS** (CMSIS_V2)
- 每个核各有一个 `defaultTask`

## 构建系统
- **工具链**: GCC (arm-none-eabi)
- **构建系统**: CMake (由 CubeMX 自动生成)
- **顶层 CMakeLists.txt** 使用 ExternalProject 分别构建 CM4 和 CM7
- 参见 `mx-generated.cmake`

## 项目目录结构
| 目录 | 说明 |
|------|------|
| `CM7/Core/` | Cortex-M7 应用 + HAL 初始化 |
| `CM4/Core/` | Cortex-M4 应用 + HAL 初始化 |
| `CM7/Core/Hardware/` | 自定义显示驱动 (LCD, LTDC, display) |
| `Common/` | 双核共享代码 (启动引导) |
| `Drivers/CMSIS/` | ARM CMSIS 头文件 |
| `Drivers/STM32H7xx_HAL_Driver/` | ST HAL 驱动库 |
| `Middlewares/Third_Party/FreeRTOS/` | FreeRTOS 源码 |
