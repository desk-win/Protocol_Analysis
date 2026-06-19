#include "my_adc_try.h"
#include "adc.h"
#include "stm32h7xx_hal_adc.h"
#include "stm32h7xx_hal_adc_ex.h"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_dma.h"
#include <stdint.h>


__attribute__((section(".ARM.__at_0x24000000"))) uint32_t adc_data;

HAL_StatusTypeDef My_ADC_Init(){
    HAL_StatusTypeDef status;
    status = HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED);
    while (status != HAL_OK);
    status = HAL_ADC_Start_DMA(&hadc1, &adc_data, 1);
    return status;
}

