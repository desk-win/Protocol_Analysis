#ifndef __MY_DMA_CATCH_H
#define __MY_DMA_CATCH_H

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_tim.h"
#include "tim.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "shared_config.h"   /* DMA_CATCH_BUF / DMA_CATCH_CTRL / SHM_DMA_CATCH_SIZE */

/* 引脚状态结构体（保留，供 SPI 等模块调用）*/
typedef struct GPIO_State {
    uint8_t PG7_State;
    uint8_t PG10_State;
    uint8_t PG12_State;
} GPIO_State;

/* 环形缓冲区读写指针（保留兼容）*/
typedef struct range_buffer {
    uint16_t head;
    uint16_t tail;
    uint32_t len;
} range_buffer;

/* RTOS 信号量：ISR 中 Give，Task 或 CM7 侧消费 */
extern SemaphoreHandle_t DMA_Catch_Semaphore;

/* ── API ── */

/* 初始化并启动 DMA_Catch（DMA 直接填共享缓冲区 DMA_CATCH_BUF）*/
void My_DMA_Catch_Init(void);

/* 停止 DMA_Catch */
void My_DMA_Catch_Stop(void);

/* 读取单个 GPIO 快照（保留兼容，SPI 模块用）*/
GPIO_State my_gpio_check(void);

/* DMA 双缓冲 ISR 回调（替代 HAL 默认回调）*/
void HAL_DMA_XferHalfCpltCallback(DMA_HandleTypeDef *hdma);
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma);

/* RTOS 任务：由 M7 的 dma_catch_enable 开关控制 */
void DMA_Catch_Ctrl_Task(void *argument);

#endif
