#ifndef __MY_DMA_CATCH_H
#define __MY_DMA_CATCH_H

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_tim.h"
#include "tim.h"
#include <stdint.h>

//引脚状态结构体
typedef struct GPIO_State{
    uint8_t PG7_State;
    uint8_t PG10_State;
    uint8_t PG12_State;
}GPIO_State;

//环形缓冲区读写指针
typedef struct range_buffer{
    uint16_t head;
    uint16_t tail;
    uint32_t len;
}range_buffer;

void My_DMA_Catch_Init(void);
GPIO_State my_gpio_check(void);

#endif
