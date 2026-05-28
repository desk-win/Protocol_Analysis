---
name: dual-core-arch
description: CM7/CM4 双核架构：各核职责划分、核间通信方式、共享资源、MPU 配置
metadata:
  type: project
---

# 双核架构

## 职责划分

### Cortex-M7 (主核, 480MHz)
- **显示系统**: LTDC + FMC SDRAM + DMA2D
- **触摸面板**: I2C2 (CTP_INT, CTP_SDA, CTP_SCL, CTP_RST)
- **定时器**: TIM3 (自定义), TIM6 (HAL Tick)
- **FMC SDRAM**: 32-bit 宽度, Bank2, 0xC0000000 (128MB) + 0xD0000000 帧缓冲区

### Cortex-M4 (从核, 240MHz)
- **摄像头**: DCMI (8-bit parallel, Hardware Sync, DMA1_Stream0 Circular)
- **定时器**: TIM7 (HAL Tick)

## 核间通信
- **HSEM (Hardware Semaphore)**: 使用 HSEM_ID_0
- **启动同步**: CM7 初始化完成后 Release HSEM → CM4 被唤醒
- **宏开关**: `DUAL_CORE_BOOT_SYNC_SEQUENCE` (两个核都定义)

## MPU 区域配置

| 核 | Region | 基址 | 大小 | 类型 | 用途 |
|----|--------|------|------|------|------|
| CM7 | 0 | 0xC0000000 | 32MB | Cacheable | FMC SDRAM Bank1 (系统内存) |
| CM7 | 1 | 0xD0000000 | 32MB | Cacheable | FMC SDRAM Bank2 (帧缓冲) |
| CM4 | 0 | 0x30000000 | 128KB | Shareable, Not Cacheable | 共享内存区域 |

## 时钟树
- **HSE**: 外部晶振 (OSC_IN/OUT)
- **LSE**: 32.768KHz 外部晶振
- **PLL1**: HSI / 4 × 60 = 960MHz VCO → 480MHz (CPU/SYSCLK), 240MHz (AHB)
- **FMC 时钟**: 240MHz (D1HCLK)
- **LTDC 像素时钟**: ~28.44MHz
- **I2C2**: 120MHz APB