#ifndef __SYS_UTIL_H
#define __SYS_UTIL_H

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
