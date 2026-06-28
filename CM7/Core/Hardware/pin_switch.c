#include "pin_switch.h"
#include "main.h"
#include "stm32h7xx_hal_gpio.h"



void Select_Pin(Pin_Select a){
    switch (a) {
        case UART_Pin:
            HAL_GPIO_WritePin(U1_A0_GPIO_Port, U1_A0_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U1_A1_GPIO_Port, U1_A1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U1_EN_GPIO_Port, U1_EN_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U2_A0_GPIO_Port, U2_A0_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U2_A1_GPIO_Port, U2_A1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U2_EN_GPIO_Port, U2_EN_Pin, GPIO_PIN_SET);
        break;

        case I2C_Pin:
            HAL_GPIO_WritePin(U1_A0_GPIO_Port, U1_A0_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U1_A1_GPIO_Port, U1_A1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U1_EN_GPIO_Port, U1_EN_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U2_A0_GPIO_Port, U2_A0_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U2_A1_GPIO_Port, U2_A1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U2_EN_GPIO_Port, U2_EN_Pin, GPIO_PIN_SET);
        break;

        case SPI_Pin:
            HAL_GPIO_WritePin(U1_A0_GPIO_Port, U1_A0_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U1_A1_GPIO_Port, U1_A1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U1_EN_GPIO_Port, U1_EN_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U2_A0_GPIO_Port, U2_A0_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U2_A1_GPIO_Port, U2_A1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U2_EN_GPIO_Port, U2_EN_Pin, GPIO_PIN_SET);
        break;

        case CAN_Pin:
            HAL_GPIO_WritePin(U1_A0_GPIO_Port, U1_A0_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U1_A1_GPIO_Port, U1_A1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U1_EN_GPIO_Port, U1_EN_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(U2_A0_GPIO_Port, U2_A0_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U2_A1_GPIO_Port, U2_A1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U2_EN_GPIO_Port, U2_EN_Pin, GPIO_PIN_SET);
        break;

        case NONE_Pin:
            HAL_GPIO_WritePin(U1_EN_GPIO_Port, U1_EN_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(U2_EN_GPIO_Port, U2_EN_Pin, GPIO_PIN_RESET);
            
        break;
    }
}

