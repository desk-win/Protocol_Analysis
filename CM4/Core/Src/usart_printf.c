#include "usart_printf.h"
#include "stm32h7xx_hal_uart.h"
#include "usart.h"
#include <stdint.h>


int fputc(int ch,FILE *file)
{
	while(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == 0);
	HAL_UART_Transmit(&huart1, (uint8_t*)&ch,1,0xff);
	return ch;
}
 

int _write(int fd, char *pBuffer, int size)
{
    
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == 0);
    HAL_UART_Transmit(&huart1, (uint8_t *)pBuffer, size, 0xff);
    return size;
}



uint8_t com_usart_send(uint8_t *tx_data, uint32_t len){
    HAL_UART_Transmit(&huart1, tx_data, len, 1000);
    return 1;
}

