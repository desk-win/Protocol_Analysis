#ifndef __MY_ADC_TRY_H
#define __MY_ADC_TRY_H


#include "stm32h7xx_hal.h"
#include "adc.h"

extern uint32_t adc_data;

HAL_StatusTypeDef My_ADC_Init(void);
//uint8_t Get_ADC_Data(uint16_t *data);

#endif
