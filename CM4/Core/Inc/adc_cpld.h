#ifndef __ADC_CPLD_H
#define __ADC_CPLD_H

#include "stm32h7xx_hal.h"
#include "tim.h"
#include "dcmi.h"
#include <stdint.h>
#include <stdbool.h>
#include "string.h"
#include "dma.h"
#include "usart_printf.h"

#define SAMPLES_PER_LINE   64u             //每行64个采样点
#define LINES_PER_FRAME    60u             //每帧60行
#define BYTES_PER_LINE     (SAMPLES_PER_LINE * 2u)       //每行128个字节
#define FRAME_TOTAL_BYTES  (BYTES_PER_LINE * LINES_PER_FRAME) //每帧总字节

// 帧头参数
#define HEADER_SYNC_AA     0xAAu
#define HEADER_SYNC_55     0x55u
#define HEADER_FLAG        0xD0u
#define HEADER_SIZE        6u     /* AA 55 帧号Lo 帧号Hi D0 校验 */

// 解析后的双通道缓冲区长度，为总帧长度的一半
#define CHANNEL_SAMPLES    (SAMPLES_PER_LINE * LINES_PER_FRAME) 
#define RAW_BUF_WORDS   (FRAME_TOTAL_BYTES / 4u)


extern uint32_t *raw_buf_0;
extern uint32_t *raw_buf_1;

extern uint8_t adc_a_buf[CHANNEL_SAMPLES];
extern uint8_t adc_b_buf[CHANNEL_SAMPLES];

extern uint8_t dcmi_halfcount,dcmi_count, dcmi_start,dma_en,dcmi_it;
extern volatile uint8_t frame_flag;
extern volatile uint32_t saved_ndtr;
extern uint8_t *buf;
extern volatile bool frame_ready;      /* 解析完一帧后置1 */
extern volatile uint16_t last_frame_id;

void CPLD_Base_Clock(uint8_t enable);
void DCMI_DoubleBuffer_Start(void);
void CPLD_Stop(void);

#endif
