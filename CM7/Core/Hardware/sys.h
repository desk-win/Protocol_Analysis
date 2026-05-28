/**
 * 系统初始化 — Cache 使能、时钟树配置
 * ===================================
 * 初始化时钟:   sys_stm32_clock_init(192, 5, 2, 4)   HSE 25MHz → PLL1 480MHz, SYSCLK=480, HCLK=240
 * 使能 Cache:   sys_cache_enable()                    I-Cache + D-Cache (强制 write-through)
 * 中断控制:     sys_intx_disable() / sys_intx_enable()
 * 低功耗:       sys_wfi_set()
 *
 * SYS_SUPPORT_OS = 0  表示 delay 模块不依赖 RTOS (FreeRTOS 通过 CMSIS-OS2 另行管理)
 */
#ifndef __SYS_H
#define __SYS_H

#include "stm32h7xx_hal.h"
#include "core_cm7.h"

#define SYS_SUPPORT_OS         0

#define      ON      1
#define      OFF     0
#define      Write_Through()    do{ *(__IO uint32_t*)0XE000EF9C = 1UL << 2; }while(0)

uint8_t get_icahce_sta(void);
uint8_t get_dcahce_sta(void);
void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset);
void sys_cache_enable(void);
uint8_t sys_stm32_clock_init(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq);

void sys_wfi_set(void);
void sys_intx_disable(void);
void sys_intx_enable(void);
void sys_msr_msp(uint32_t addr);

#endif
