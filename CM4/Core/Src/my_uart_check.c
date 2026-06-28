#include "my_uart_check.h"
#include "main.h"
#include "my_dwt_count.h"
#include "portmacro.h"
#include "projdefs.h"
#include "shared_buf.h"
#include "shared_config.h"
#include "stm32_hal_legacy.h"
#include "stm32h747xx.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_uart.h"
#include "stm32h7xx_hal_uart_ex.h"
#include "usart.h"
#include <stdint.h>
#include <string.h>


/****************************************************************************
*基础收发：
*    发送功能：
*    ├── 单次发送
*    ├── 循环发送（可配置间隔时间）
*    ├── 发送任意长度数据帧
*    └── 发送完成回调通知
*    接收功能：
*    ├── 不定长数据接收（IDLE 空闲中断触发）
*    ├── DMA 接收，减少 CPU 占用
*    ├── 接收缓冲区管理（环形缓冲区）
*    └── 接收超时检测
*参数配置：
*    ├── 波特率：常用预设（9600/19200/38400/57600/115200/230400/460800/921600）
*    ├── 数据位：8bit / 9bit
*    ├── 停止位：1位 / 1.5位 / 2位
*    ├── 奇偶校验：无 / 奇校验 / 偶校验
*    └── 流控：无 / RTS-CTS 硬件流控
*错误检测：
*    ├── 帧错误（FE）：停止位不正确
*    ├── 奇偶校验错误（PE）：校验位不匹配
*    ├── 溢出错误（ORE）：接收缓冲区满
*    ├── 噪声错误（NE）：采样到噪声
*    └── 错误计数统计（各类错误分别计数）
*协议层分析：
*    ├── 波特率自动检测（这个不知道有没有必要做，因为手动设置波特率正确率高一点）
*    ├── 帧完整性检查（帧头帧尾验证）
*    ├── 连续错误率统计（滑动窗口）
*    └── 最大/最小/平均帧间隔统计
*
***************************************************************************************/

My_UART_State now_uart_state;
UART_Cycle_Tx_t cycle_tx_ctrl;
RingBuffer_t rx_ring_buf;
uint8_t rx_uart_buffer[UART_RXDMA_LEN];            //用来接收DMA搬运数据的缓冲区
UART_ErrorStats_t uart_errors;
UART_Pact_t uart_analysis;
uint8_t if_uart_rxok = 0;        //接收完成标志（0=未完成，2=完成），供外部轮询读取

SemaphoreHandle_t uart_rxcallback_semaphore = NULL;
SemaphoreHandle_t uart_txcallback_semaphore = NULL;
/***********************************工具函数***********************************/
/**
*   @brief 这个函数用来管理缓冲区，如果缓冲区没有满就将数据放进去并将指针指向下一个位置
*   @param data:要写入的一个数据
*/
void UART_RingBuffer_Push(uint8_t data) {
    uint16_t next_head = (rx_ring_buf.head + 1) % RING_BUFFER_LEN;
    if (next_head == rx_ring_buf.tail) {
        //如果满了,覆盖掉最老的数据
        rx_ring_buf.tail = (rx_ring_buf.tail + 1)%RING_BUFFER_LEN;
    }
    rx_ring_buf.buffer[rx_ring_buf.head] = data;
    rx_ring_buf.head = next_head;

}

/**
*   @brief 这个函数用来从环形缓冲区里面读取数据
*   @param pData：用来放数据的缓冲区
*   @param len:数据长度
*   @retval uint32_t 实际读取到的数据长度
*/
uint32_t My_UART_Read_RingBuffer(uint8_t *pDest, uint32_t len) {
    uint32_t read_len = 0;
    while (rx_ring_buf.tail != rx_ring_buf.head && read_len < len) {
        pDest[read_len++] = rx_ring_buf.buffer[rx_ring_buf.tail];
        rx_ring_buf.tail = (rx_ring_buf.tail + 1) % RING_BUFFER_LEN;
    }
    return read_len;
}

/******************************************初始化和参数配置*******************************************/
//UART没有主从之分，所以这部分配置简单一点，可以配置的只有波特率、数据位、停止位、校验位、硬件控
//制流（这个我觉得其实没有必要）。使用UART的空闲中断实现不定长数据的接收

/**
*   @brief 这个函数用来初始化uart的一些配置
*/
void My_UART_Init(void) {
    memset(&now_uart_state, 0, sizeof(now_uart_state));
    memset(&cycle_tx_ctrl, 0, sizeof(cycle_tx_ctrl));
    rx_ring_buf.head = 0;
    rx_ring_buf.tail = 0;
    /* 裸机环境下重新启用 USART6 RX DMA（无 FreeRTOS 冲突）*/
    HAL_UARTEx_ReceiveToIdle_DMA(&huart6, rx_uart_buffer, UART_RXDMA_LEN);
}

/**
*   @brief 这个函数可以让用户进行参数配置
*   @param baud:波特率
*   @param data_len:数据位8bit/9bit
*   @param stop_bits:停止位
*   @param parity:奇偶校验位
*   @param flow_ctrl:硬件控制流
*/
HAL_StatusTypeDef UART_Param_Change(uint32_t baud, uint32_t data_len, uint32_t stop_bits, uint32_t parity, uint32_t flow_ctrl) {
    //停止当前的所有 DMA 传输和空闲中断
    HAL_UART_DMAStop(&huart6);
    __HAL_UART_DISABLE_IT(&huart6, UART_IT_IDLE);

    //解除初始化
    if (HAL_UART_DeInit(&huart6) != HAL_OK) return HAL_ERROR;

    //赋新参数
    huart6.Init.BaudRate = baud;
    huart6.Init.WordLength = data_len;
    huart6.Init.StopBits = stop_bits;
    huart6.Init.Parity = parity;
    huart6.Init.HwFlowCtl = flow_ctrl;
    huart6.Init.Mode = UART_MODE_TX_RX;

    //重新初始化硬件
    if (HAL_UART_Init(&huart6) != HAL_OK) return HAL_ERROR;

    //重新启动 DMA + 空闲中断接收
    HAL_UARTEx_ReceiveToIdle_DMA(&huart6, rx_uart_buffer, UART_RXDMA_LEN);

    return HAL_OK;
}


/****************************************接收发送部分****************************************/
/**
*   @brief 这个函数用来单次发送数据
*   @param tx_data:要发送的数据
*   @param len:数据长度
*/
void My_UART_Send_Single(uint8_t *tx_data, uint32_t len){
    // 强制等待上一次发送完成（规避 BUSY 错误）
    while (huart6.gState == HAL_UART_STATE_BUSY_TX);
    now_uart_state.tx_notice = 0;
    // 关闭正在进行的循环发送，优先单次发送
    cycle_tx_ctrl.is_running = 0;

    HAL_UART_Transmit_IT(&huart6, tx_data, len);
}

/**
*   @brief 这个函数用来初始化循环发送数据配置
*   @param tx_data:要发送的数据
*   @param len:数据长度
*   @param occur:发送次数
*   @param times:间隔时间单位ms
*/
void My_UART_Send_Cycle(uint8_t *tx_data, uint32_t len, uint16_t occur, uint32_t times){
    cycle_tx_ctrl.tx_data = tx_data;
    cycle_tx_ctrl.len = len;
    cycle_tx_ctrl.remain_count = occur;
    cycle_tx_ctrl.interval_ms = times;
    cycle_tx_ctrl.last_tick = 0;       // 确保第一次立刻发送
    cycle_tx_ctrl.is_running = 1;
    now_uart_state.tx_notice = 0;
}

/**
*   @brief 这个函数放在主循环里面用来实现uart的循环发送
*/
void My_UART_LoopTask(void) {
    if (!cycle_tx_ctrl.is_running) return;

    uint32_t current_tick = Get_Sys_us();          //记录当前时间戳
    
    // 时间间隔到达，且串口处于空闲状态
    if ((current_tick - cycle_tx_ctrl.last_tick >= cycle_tx_ctrl.interval_ms) && 
        (huart6.gState != HAL_UART_STATE_BUSY_TX)) 
    {
        if (cycle_tx_ctrl.remain_count > 0) {
            HAL_UART_Transmit_IT(&huart6, cycle_tx_ctrl.tx_data, cycle_tx_ctrl.len);
            cycle_tx_ctrl.last_tick = current_tick;
            cycle_tx_ctrl.remain_count--;
        } else {
            // 次数发送完毕
            cycle_tx_ctrl.is_running = 0;
            now_uart_state.tx_notice = 1; // 触发发送完成通知
        }
    }
}

/**
*   @brief 这个函数用来更新错误滑动窗口，当有错误的时候，无论是哪种错误，在窗口写1
*   @param if_error:有错误置1，没有置0
*/
void Update_Error_Window(uint8_t if_error){
    //将数据放入窗口
    uart_errors.error_history[uart_errors.window_index] = if_error;
    //每次窗口满了之后用新数据盖掉旧数据
    uart_errors.window_index = (uart_errors.window_index + 1)%ERROR_WINDOW_SIZE;

    //计算当前错误率
    uint32_t sum = 0;
    for (uint8_t i = 0; i<ERROR_WINDOW_SIZE; i++) {
        sum += uart_errors.error_history[i];
    }
    uart_errors.recent_error_rate = (float_t)sum/ERROR_WINDOW_SIZE;
}

//hal库硬件错误回调函数
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){
    if (huart->Instance == USART6) {
        uint32_t err = huart->ErrorCode;
        if (err & HAL_UART_ERROR_FE) {
            uart_errors.frame_count++;   // 帧错误
            uart_errors.frame_error = 1;
        }
        if (err & HAL_UART_ERROR_PE) {
            uart_errors.parity_count++;   // 奇偶校验错误
            uart_errors.frame_error = 1;
        }
        if (err & HAL_UART_ERROR_ORE) {
            uart_errors.over_count++;  // 接收溢出错误
            uart_errors.frame_error = 1;
        }
        if (err & HAL_UART_ERROR_NE) {
            uart_errors.noise_count++;   // 噪声错误
            uart_errors.frame_error = 1;
        }
        Update_Error_Window(uart_errors.frame_error);
        uart_errors.frame_error = 0;
        // 发生硬件错误后，HAL库内部会关闭接收，必须手动重启DMA接收
        HAL_UARTEx_ReceiveToIdle_DMA(&huart6, rx_uart_buffer, UART_RXDMA_LEN);
    }

}

/***************************************协议层分析*****************************************/

/**
*   @brief 这个函数用来比较帧头帧尾是否符合（如果传输的数据是有帧头帧尾的话）
*   @param data:接收到的数据缓冲区
*   @param len:数据长度
*   @param form_data:带有协议数据的结构体
*   @retval uint8_t,0为不符合，1为符合
*/
uint8_t Check_Form(uint8_t *data, uint32_t len, UART_Pact_t form_data){
    if ((data[0] != form_data.form_head) || (data[len-1] != form_data.form_tail)) {
        //如果有帧头或者帧尾不符合
        return 0;
    }
    return 1;
}

//接收完一段不定长数据之后会进入这个空闲中断回调函数，Size 是硬件自动计算出来的本次收到的实际不定长数据长度
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){
    if (huart->Instance == USART6) {
        //记录实际不定长数据长度
        uart_analysis.rx_frame_size = Size;
        //记录当前时间
        uart_analysis.current_frame_tick = Get_Sys_us();
        
        //将接收数据写入环形缓冲区
        for (uint16_t i = 0; i < uart_analysis.rx_frame_size; i++) {
            UART_RingBuffer_Push(rx_uart_buffer[i]);
        }
        //开启下次中断
        HAL_UARTEx_ReceiveToIdle_DMA(&huart6, rx_uart_buffer, UART_RXDMA_LEN);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(uart_rxcallback_semaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// 发送完成中断回调
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        // 如果不是循环发送模式，或者循环发送刚好结束，通知应用层
        if (!cycle_tx_ctrl.is_running) {
            now_uart_state.tx_notice = 1; 
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(uart_txcallback_semaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

/**
*   @brief UART回调处理任务
*
*
*
**/
void UART_Callback_Task(void *argument){
    //创建二值信号量，在接收回调函数里面释放
    while (1) {
        if (xSemaphoreTake(uart_rxcallback_semaphore, portMAX_DELAY) == pdPASS) {
            
            //触发接收完成通知标志
            now_uart_state.rx_notice = 1;   
            if (uart_analysis.success_count == 0) {
                //这里是第一帧
                uart_analysis.min_interval = 0xFFFFFFFF;
                uart_analysis.max_interval = 0;
            }else {
                //两帧之间的时间间隔
                uint32_t interval = uart_analysis.current_frame_tick - uart_analysis.last_frame_tick;
                //找出最大最小帧间隔
                if(interval > uart_analysis.max_interval) uart_analysis.max_interval = interval;
                if(interval < uart_analysis.min_interval) uart_analysis.min_interval = interval;
                //总帧间隔
                uart_analysis.total_interval += interval;
                uart_analysis.avg_interval = uart_analysis.total_interval/uart_analysis.success_count;

            }
            uart_analysis.last_frame_tick = uart_analysis.current_frame_tick;
            //如果数据是有格式的
            if (uart_analysis.if_form == 1) {
                if (Check_Form(rx_uart_buffer,uart_analysis.rx_frame_size,uart_analysis)) {
                    uart_analysis.success_count++;
                    //处理了一个无错误帧
                    Update_Error_Window(0);
                }else {
                    uart_analysis.error_count++;
                    Update_Error_Window(1);
                }
            }else {
                if (uart_errors.frame_error == 0) {
                    uart_analysis.success_count++;
                    //处理了一个无错误帧
                    Update_Error_Window(0);
                }else if (uart_errors.frame_error == 1) {
                    uart_analysis.error_count++;
                    Update_Error_Window(1);
                }
            }
            if_uart_rxok = 2;
            //M4将数据推送到共享环形缓冲区
            //先告诉M7包头格式
            shm_push(0xFD);
            shm_push(0xAA);             //uart的帧头
            shm_push_u16(uart_analysis.rx_frame_size);      // 数据长度
            shm_push_u32(uart_analysis.total_interval);
            shm_push_u32(uart_analysis.success_count);
            shm_push_u32(uart_analysis.error_count);
            shm_push(ERROR_WINDOW_SIZE);
            //推送接收的数据
            uint8_t tempo_buffer[uart_analysis.rx_frame_size];
            My_UART_Read_RingBuffer(tempo_buffer, uart_analysis.rx_frame_size);
            shm_push_buf(tempo_buffer, uart_analysis.rx_frame_size);
            shm_push_buf(uart_errors.error_history, ERROR_WINDOW_SIZE);
            
        }else if (xSemaphoreTake(uart_txcallback_semaphore, 0) == pdPASS) {
            /* TX 完成 → 通知 M7 */
            __DMB();
            SHM_STATUS->protocol_done  = 1;
            SHM_STATUS->protocol_error = 0;
            HAL_HSEM_Take(HSEM_ID_DONE, 0);
            HAL_HSEM_Release(HSEM_ID_DONE, 0);
        }

        /* 检查 M7 是否有待发送数据（主机发送模式）*/
        {
            uint16_t tx_len = *SHM_TX_LEN;
            if (tx_len > 0 && tx_len <= SHM_TX_BUF_SIZE) {
                uint8_t tx_data[SHM_TX_BUF_SIZE];
                memcpy(tx_data, (const void*)SHM_TX_BUF, tx_len);
                *SHM_TX_LEN = 0;  /* 标记已消费 */
                My_UART_Send_Single(tx_data, tx_len);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
