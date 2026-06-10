#ifndef __MY_UART_CHECK_H
#define __MY_UART_CHECK_H

#include "stm32h7xx_hal.h"
#include "usart.h"
#include "stdint.h"
#include "string.h"
#include <stdint.h>
#include <sys/types.h>

#define UART_RXDMA_LEN      256   // DMA 单次接收缓冲区大小（够容纳一帧最大数据即可）
#define RING_BUFFER_LEN     1024  // 环形缓冲区大小
#define ERROR_WINDOW_SIZE   10 // 错误滑动窗口大小，统计最近10次传输的错误率
//串口状态结构体
typedef struct {
    uint8_t tx_notice;
    uint8_t rx_notice;
}My_UART_State;

//循环发送控制结构体
typedef struct {
    uint8_t *tx_data;             // 待发送数据指针
    uint32_t len;                 // 数据长度
    uint16_t remain_count;        // 剩余发送次数
    uint32_t interval_ms;         // 间隔时间
    uint32_t last_tick;           // 上一次发送的时间戳
    uint8_t is_running;          // 是否正在循环发送
} UART_Cycle_Tx_t;

//接收数据环形缓冲区结构体
typedef struct {
    uint8_t buffer[RING_BUFFER_LEN];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer_t;

//错误计数统计结构体
typedef struct {
    uint32_t frame_count;   // 帧错误计数
    uint32_t parity_count;   // 奇偶校验错误计数
    uint32_t over_count;  // 溢出错误计数
    uint32_t noise_count;   // 噪声错误计数
    
    // 滑动窗口连续错误率统计
    uint8_t error_history[ERROR_WINDOW_SIZE];    
    uint8_t window_index;       //窗口指针
    float recent_error_rate; // 统计窗口内错误率
} UART_ErrorStats_t;

//协议分析部分结构体
typedef struct {
    uint8_t if_form;           // 判断有没有帧结构，0没有
    uint32_t last_frame_tick;  // 上一帧接收完成的时间戳
    uint32_t max_interval;     // 最大帧间隔 (ms)
    uint32_t min_interval;     // 最小帧间隔 (ms)
    uint32_t total_interval;   // 总帧间隔时间（用于算平均值）
    uint32_t success_count;    // 成功接收的有效帧计数
    uint32_t avg_interval;     // 平均帧间隔 (ms)
    
    uint8_t form_head;          // 帧头
    uint8_t form_tail;          // 帧尾
    uint32_t error_count;       // 帧完整性错误计数
} UART_Pact_t;

void My_UART_Init(void);
HAL_StatusTypeDef UART_Param_Change(uint32_t baud, uint32_t data_len, uint32_t stop_bits, uint32_t parity, uint32_t flow_ctrl);
void My_UART_Send_Single(uint8_t *tx_data, uint32_t len);
void My_UART_Send_Cycle(uint8_t *tx_data, uint32_t len, uint16_t occur, uint32_t times);
void My_UART_LoopTask(void);
void Update_Error_Window(uint8_t if_error);
uint8_t Check_Form(uint8_t *data, uint32_t len, UART_Pact_t form_data);

#endif
