---
name: task-layout
description: FreeRTOS 任务布局：任务名称、优先级、堆栈大小、职责
metadata:
  type: project
---

# RTOS 任务布局

## CM7 (Cortex-M7)

| 属性 | 值 |
|------|-----|
| RTOS | FreeRTOS (CMSIS_V2) |
| FPU | 启用 |
| Heap | 0x200 |
| Stack | 0x400 |
| SysTick 替代 | TIM6 (HAL Tick) |

### defaultTask

| 属性 | 值 |
|------|-----|
| 入口函数 | `StartDefaultTask` |
| 优先级 | osPriorityNormal (24) |
| 堆栈大小 | 512 bytes (128 × 4) |
| 职责 | **LCD 显示测试**：初始化 LCD → 颜色循环 → 棋盘格 → 边框 → 字符显示 |

## CM4 (Cortex-M4)

| 属性 | 值 |
|------|-----|
| RTOS | FreeRTOS (CMSIS_V2) |
| FPU | 启用 |
| Heap | 0x200 |
| Stack | 0x400 |
| SysTick 替代 | TIM7 (HAL Tick) |

### defaultTask

| 属性 | 值 |
|------|-----|
| 入口函数 | `StartDefaultTask` |
| 优先级 | osPriorityNormal (24) |
| 堆栈大小 | 512 bytes (128 × 4) |
| 职责 | **空闲循环** (待开发，预留给摄像头处理) |

## 任务同步（待实现）
当前两个 defaultTask 之间没有同步机制。CM4 的 DCMI DMA 捕获图像后需要通过队列或消息队列传递给 CM7 进行显示。
