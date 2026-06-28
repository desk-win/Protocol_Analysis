#ifndef __PIN_SWITCH_H
#define __PIN_SWITCH_H


#include "stm32h7xx_hal.h"

typedef enum{
    UART_Pin,
    I2C_Pin,
    SPI_Pin,
    CAN_Pin,
    NONE_Pin
} Pin_Select;

void Select_Pin(Pin_Select a);

#endif

