#ifndef __MY_SPI_CHECK_H
#define __MY_SPI_CHECK_H

#include "stm32h7xx_hal.h"
#include "spi.h"
#include <math.h>
#include <stdint.h>
#include "string.h"
#define DMA_BUFFER_LEN      256
#define SPI_RANGE_BUFFER_LEN    2048

//用来指示当前作为从机还是主机的标志位
typedef enum {
    MY_SPI_MASTER,
    MY_SPI_SLAVE
}My_SPI_Mode;

//用来统计各种错误次数的结构体
typedef struct{
    uint8_t mode_error;     
    uint8_t crc_error;
    uint8_t over_error;
    uint8_t DMA_error;
    uint32_t error_code;
} My_SPI_Error;

//接收数据的环形缓冲区
typedef struct{
    uint16_t buffer_head;
    uint16_t buffer_tail;
    uint8_t range_buffer[SPI_RANGE_BUFFER_LEN];
} SPI_Range_Buffer;

//用来放spi的配置设置
typedef struct{
    My_SPI_Mode spi_role;       //当前的主从模式
    uint8_t cs_to_clk;          //配置cs信号到clk之间的延时时间
    My_SPI_Error spi_error;
    uint8_t time_mode;          //时钟模式
    uint16_t baudrateprescaler; //时钟分频配置
    uint8_t cs_polarity;
} My_SPI_Deploy;

//协议分析方面的结构体
typedef struct{
  uint32_t cs_low_tick;         //cs引脚拉低的时刻
  uint32_t cs_hight_tick;       //cs引脚拉高的时刻
  uint32_t cs_gap_tick;         //cs脉冲宽度  
  uint16_t spi_total_frame;     //总帧数
  uint16_t spi_success_frame;   //成功帧数
  uint16_t spi_fail_frame;      //失败帧数
  float_t transmit_success_rate;//传输成功率
  uint32_t spi_frame_gap;       //帧间隔
} My_SPI_Analyse;

extern SPI_Range_Buffer spi_range_buffer;
extern My_SPI_Analyse spi_analyse[20];

extern uint8_t rx_spi_buffer[DMA_BUFFER_LEN];

extern My_SPI_Deploy spi_deploy;
extern uint8_t if_busy;   /* main 循环 master 分支查 busy（IT 非阻塞）*/

void CS_Pin_State(uint8_t level);
void CS_Switch_To_Exit(void);
uint8_t SPI_RangeBuffer_Wirte(uint8_t data);
uint32_t SPI_RangeBuffer_Read(uint8_t *data, uint32_t data_len);
uint8_t SPI_PutData_To_Buffer(void);
void My_SPI_Init(uint8_t time_mode, My_SPI_Mode role, uint8_t cs_delay, uint16_t baudrate, uint8_t cs_polarity);
void Switch_SPI_Mode(My_SPI_Mode spi_mode);
void My_SPI_Send(uint8_t *data, uint32_t len);
void My_SPI_SendReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t len);

uint32_t spi_datasize_from_u8(uint8_t bits);       /* UI datasize(4-8) → HAL SPI_DATASIZE_NBIT */
uint16_t spi_prescaler_from_baud(uint32_t baud);   /* UI baud 档位 → prescaler（master 用）*/


#endif
