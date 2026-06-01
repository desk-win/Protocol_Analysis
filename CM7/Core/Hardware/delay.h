/**
 * 延时函数 — 基于 SysTick 轮询, 不依赖中断, 不占用 SysTick 中断
 * ==========================================================
 * 初始化:   delay_init(480)          参数 = 系统频率 MHz (480MHz)
 * 微秒延时: delay_us(nus)            基于 SysTick->VAL 轮询, 无需中断
 * 毫秒延时: delay_ms(nms)            内部调用 delay_us
 * HAL 延时: HAL_Delay(ms)            重映射到 delay_ms (解决 CubeMX TIM6 时基冲突)
 *
 * 注意: delay_init() 必须在 sys_stm32_clock_init() 之后调用,
 *       且必须在 FreeRTOS 调度器启动前调用。
 */
#ifndef __DELAY_H
#define __DELAY_H

#include "sys_util.h"

void delay_init(uint16_t sysclk);
void delay_ms(uint16_t nms);
void delay_us(uint32_t nus);
void HAL_Delay(uint32_t Delay);

#endif
