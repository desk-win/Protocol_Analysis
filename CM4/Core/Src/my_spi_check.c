#include "my_spi_check.h"
#include "cmsis_os2.h"
#include "my_dwt_count.h"
#include "spi.h"
#include "stm32_hal_legacy.h"
#include "stm32h747xx.h"
#include "stm32h7xx_hal_cortex.h"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_exti.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_spi.h"
#include "my_dma_catch.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*******************************实现的功能*********************************
*主机模式：
*    ├── 全双工收发（同时发送 MOSI 和接收 MISO）
*    ├── 发送任意字节序列，实时显示 MISO 返回数据
*    ├── 只发不收（单向发送）
*    ├── 软件 CS
*    ├── 每帧传输前后自动拉低/拉高 CS
*    ├── 可配置 CS 到 SCLK 的建立时间
*从机模式：
*    ├── 接收主机发来的数据并缓存
*    ├── 主机发起读时返回
*    ├── CS 下降沿触发接收开始
*    ├── CS 上升沿触发帧结束处理
*    └── 支持不定长帧（CS 控制帧边界）
*参数配置：
*    ├── 时钟极性 CPOL：0（空闲低）/ 1（空闲高），时钟相位 CPHA：0（第一边沿）/ 1（第二边沿）
*    ├── 时钟频率：从 PCLK 分频
*    ├── 模式切换
*    └── CS 有效极性：低有效 / 高有效
*错误检测：
*    ├── 溢出错误（OVR）：接收缓冲区满
*    ├── 模式错误（MODF）：NSS 异常（主机模式）
*    ├── 帧格式错误（FRE）：TI 模式下帧错误
*    ├── CRC 错误（CRCERR）：使能 CRC 时数据校验失败
*协议层分析：
*    ├── cs脉冲宽度测量
*    ├── 帧间隔时间统计
*    └── 统计成功通信帧数，计算通信成功率
*
************************************************************************/

/******************************************************************************
*   首先调用my_spi_init进行初始化，可以配置的参数：（从机：spi模式、cs延时）（主机：
*   spi模式、cs延时、CPOL / CPHA、时钟分频、cs引脚有效电平）。从机很多参数是固定的，
*   因为感觉配置了也没什么作用
*
*   通过Switch_SPI_Mode函数切换spi主从模式
*
*   主机模式：
*   此时cs引脚配置为推挽输出，发送使用My_SPI_Send函数，接收使用My_SPI_SendReceive函数
*   或者都使用My_SPI_SendReceive函数，因为spi是全双工的。
*
*   从机模式：
*   使用软件cs，cs引脚配置为外部中断，当检测到被片选或者取消片选的时候做出对应操作。
*   所有的协议层分析都是在作为从机的时候使用的。使用的接收发送函数是HAL_SPI_TransmitReceive_DMA
*   因为在实测的时候，软件cs在触发外部中断的时候才开启HAL_SPI_TransmitReceive_DMA的话
*   会错过数据，所以在初始化的时候就先开启接收中断然后关闭spi外设，在接收到cs片选信号的时候
*   才开启外设。接收到的数据先放在rx_spi_buffer里面，然后再存入spi_range_buffer.range_buffer
*
*   错误检测和协议分析：
*   错误检测使用的的hal库自己的HAL_SPI_ErrorCallback，后面如果要加入什么功能再添加进去。
*   协议层分析，调用spi_analyse.cs_gap_tick可以得到cs脉冲宽度，调用spi_analyse.transmit_success_rate
*   得到传输成功率，调用spi_analyse.spi_frame_gap得到帧间隔
******************************************************************************/

__attribute__((section(".RAM_D3"))) uint8_t rx_spi_buffer[DMA_BUFFER_LEN];
__attribute__((section(".RAM_D3"))) uint8_t tx_spi_buffer[DMA_BUFFER_LEN];

uint8_t if_busy = 0;                            //作为主机发送数据的时候检查是否发送完成标志位
volatile uint8_t if_rx_finish = 0;              //全局变量用来反应接收是否完成（作为从机的时候使用），0表示未完成
My_SPI_Error spi_error_code;                    //这个结构体用来记录各种错误
My_SPI_Deploy spi_deploy;
SPI_Range_Buffer spi_range_buffer;               //用来记录接收环形缓冲区状态
My_SPI_Analyse spi_analyse[20] = {0};
uint8_t analyse_index = 0;                      //spi_analyse[20]结构体数组的索引
/****************************************工具函数*************************************/
/**
*   @brief 作为主机时cs引脚状态函数
*   @param level:1为高电平，0为低电平
*/
void CS_Pin_State(uint8_t level){
    if (level) {
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);
    }else {
        HAL_GPIO_WritePin(GPIOG,  GPIO_PIN_10,  GPIO_PIN_RESET);
    }
}

/**
*   @brief 作为从机的时候用来向环形缓冲区写入数据
*   @param data:写入的数据
*   @retval 返回1表示有数据被覆盖了
**/
uint8_t SPI_RangeBuffer_Wirte(uint8_t data){
    uint8_t if_cover = 0;
    if ((spi_range_buffer.buffer_head + 1)%SPI_RANGE_BUFFER_LEN == spi_range_buffer.buffer_tail) {
        //如果缓冲区满了，丢掉最老的一个数据
        spi_range_buffer.buffer_tail = (spi_range_buffer.buffer_tail + 1)%SPI_RANGE_BUFFER_LEN;
        if_cover = 1;
    }
    spi_range_buffer.range_buffer[spi_range_buffer.buffer_head] = data;
    spi_range_buffer.buffer_head = (spi_range_buffer.buffer_head + 1)%SPI_RANGE_BUFFER_LEN;
    return if_cover;
}

/**
*   @brief 从环形缓冲区里面读取数据的函数
*   @param data：用来放数据的缓冲区
*   @param data_len:接收指针的大小
*   @retval 0表示缓冲区里面没有数据，1表示正常读取
**/
uint8_t SPI_RangeBuffer_Read(uint8_t *data, uint32_t data_len){
    uint8_t if_empty = 1;
    uint32_t data_index = 0;                //索引
    if (spi_range_buffer.buffer_tail != spi_range_buffer.buffer_head) {
        //当缓冲区里面还有数据的时候
        while (spi_range_buffer.buffer_tail != spi_range_buffer.buffer_head) {
            data[data_index] = spi_range_buffer.range_buffer[spi_range_buffer.buffer_tail];
            data_index++;
            if(data_index >= data_len) break;
            spi_range_buffer.buffer_tail = (spi_range_buffer.buffer_tail + 1)%SPI_RANGE_BUFFER_LEN;
        }
    }else {
        if_empty = 0;
    }
    return if_empty;
}

void SPI_DMAStop_Manual(SPI_HandleTypeDef *hspi)
{
    // 1. 关闭DMA stream
    if (hspi->hdmarx != NULL)
    {
        HAL_DMA_Abort(hspi->hdmarx);
    }
    if (hspi->hdmatx != NULL)
    {
        HAL_DMA_Abort(hspi->hdmatx);
    }

    // 2. 关闭SPI的DMA请求
    CLEAR_BIT(hspi->Instance->CFG1, SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);

    // 3. 等待SPI不忙，然后关闭SPI
    while (HAL_IS_BIT_SET(hspi->Instance->SR, SPI_SR_EOT));
    __HAL_SPI_DISABLE(hspi);
    
    // 4. 清除FIFO
    SET_BIT(hspi->Instance->CR1, SPI_CR1_CSUSP);
    
    // 5. 恢复HAL状态机，否则下次调用HAL函数会返回HAL_BUSY
    hspi->State = HAL_SPI_STATE_READY;
    hspi->ErrorCode = HAL_SPI_ERROR_NONE;
}

/*************************************配置更改和模式切换**************************************/
/**
*   @brief 配置时钟模式相位的函数CPOL / CPHA（时钟极性和相位）
*   @param mode:模式0~3
**/
void SPI_Time_Mode(uint8_t mode){
    switch (mode) {
        case 0:
            hspi6.Init.CLKPolarity = SPI_POLARITY_LOW;
            hspi6.Init.CLKPhase = SPI_PHASE_1EDGE;
        break;
        case 1:
            hspi6.Init.CLKPolarity = SPI_POLARITY_LOW;
            hspi6.Init.CLKPhase = SPI_PHASE_2EDGE;
        break;
        case 2:
            hspi6.Init.CLKPolarity = SPI_POLARITY_HIGH;
            hspi6.Init.CLKPhase = SPI_PHASE_1EDGE;
        break;
        case 3:
            hspi6.Init.CLKPolarity = SPI_POLARITY_HIGH;
            hspi6.Init.CLKPhase = SPI_PHASE_2EDGE;
        break;
    }
}

/**
*   @brief 配置时钟分频
*   @param prrscaler:2\4\8\16\32\64\128\256
**/
void SPI_BaudRatePrescaler(uint16_t prrscaler){
    switch (prrscaler) {
        case 2:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2; break;
        case 4:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; break;
        case 8:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; break;
        case 16:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; break;
        case 32:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; break;
        case 64:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; break;
        case 128:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128; break;
        case 256:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; break;
        default:hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; break;
    }
}
/**
*   @brief 将cs引脚配置为推挽输出并关闭外部中断
**/
void CS_Switch_To_Output(){
    //将外部中断关闭
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);

    //在主机模式下，将cs引脚配置为推挽输出
    GPIO_InitTypeDef CS_Pin_Struct={0};
    CS_Pin_Struct.Mode = GPIO_MODE_OUTPUT_PP;
    CS_Pin_Struct.Pin = GPIO_PIN_10;
    CS_Pin_Struct.Pull = GPIO_PULLUP;
    CS_Pin_Struct.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GPIOG, &CS_Pin_Struct);
}

/**
*   @brief cs引脚配置为外部中断模式
*
**/
void CS_Switch_To_Exit(){
    //将cs引脚重新配置为输入模式
    GPIO_InitTypeDef CS_Pin_Struct={0};
    CS_Pin_Struct.Pin = GPIO_PIN_10;
    CS_Pin_Struct.Mode = GPIO_MODE_IT_RISING_FALLING;
    CS_Pin_Struct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOG, &CS_Pin_Struct);
    //开启外部中断
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
*   @brief 初始化spi操作，配置的有有效数据的4个模式、主从模式、cs信号到第一个时钟边沿的延时时间、
*   时钟频率、cs引脚有效电平（作为主机的时候配置）。使用软件cs，主机的时候为推挽输出，从机的时候为
*   外部中断，双边沿触发。
*   为了方便和屏幕对接，传入参数改为void，直接通过更改全局结构体spi_deploy里面的time_mode、spi_role、
*   cs_to_clk、baudrateprescaler、spi_error.error_code、cs_polarity参数来进行配置
**/
void My_SPI_Init(uint8_t time_mode, My_SPI_Mode role, uint8_t cs_delay, uint16_t baudrate, uint8_t cs_polarity){
    //先将值赋到配置结构体上
    spi_deploy.time_mode = time_mode;           //数据读取状态
    spi_deploy.spi_role = role;                 //主从模式状态
    spi_deploy.cs_to_clk = cs_delay;            //cs延时
    spi_deploy.baudrateprescaler = baudrate;    //波特率分频
    spi_deploy.spi_error.error_code = 0;        //错误标识
    spi_deploy.cs_polarity = cs_polarity;       //cs有效电平
    //如果当前为从机模式
    if (spi_deploy.spi_role == MY_SPI_SLAVE) {
        HAL_SPI_DeInit(&hspi6);

        hspi6.Init.Mode = SPI_MODE_SLAVE;
        hspi6.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;          //作为从机的时候cs只有低电平有效
        hspi6.Init.MasterSSIdleness = spi_deploy.cs_to_clk;     //配置cs信号到第一个时钟的延迟时间
        hspi6.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
        SPI_Time_Mode(spi_deploy.time_mode);

        HAL_SPI_Init(&hspi6);
        //在初始化的时候就开启了接收发送中断
        HAL_SPI_TransmitReceive_DMA(&hspi6, tx_spi_buffer, rx_spi_buffer, DMA_BUFFER_LEN);
        CS_Switch_To_Exit();
    }else if (spi_deploy.spi_role == MY_SPI_MASTER) {                           //如果为主机模式
        HAL_SPI_DeInit(&hspi6);                                           //反初始化

        hspi6.Init.Mode = SPI_MODE_MASTER;                                      //主机模式
        if(spi_deploy.cs_polarity == 0) hspi6.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;     //cs引脚有效电平
        else hspi6.Init.NSSPolarity = SPI_NSS_POLARITY_HIGH;
        SPI_Time_Mode(spi_deploy.time_mode);                               //配置CPOL / CPHA
        hspi6.Init.MasterSSIdleness = spi_deploy.cs_to_clk;                     //cs延迟
        hspi6.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
        SPI_BaudRatePrescaler(spi_deploy.baudrateprescaler);          //频率分频

        HAL_SPI_Init(&hspi6);                                              //重新初始化
        CS_Switch_To_Output();
    }
}

/**
*   @brief 这个函数用来切换spi的主从模式
*   @param spi_mode:枚举变量
**/
void Switch_SPI_Mode(My_SPI_Mode spi_mode){
    //关闭传输和解除配置
    SPI_DMAStop_Manual(&hspi6);
    HAL_SPI_DeInit(&hspi6);
    //保留原来的配置
    SPI_Time_Mode(spi_deploy.time_mode);
    SPI_BaudRatePrescaler(spi_deploy.baudrateprescaler);
    hspi6.Init.MasterSSIdleness = spi_deploy.cs_to_clk;

    //如果选择主机模式
    if (spi_mode == MY_SPI_MASTER) {
        hspi6.Init.Mode = SPI_MODE_MASTER;
        if(spi_deploy.cs_polarity == 0) hspi6.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;     //cs引脚有效电平
        else hspi6.Init.NSSPolarity = SPI_NSS_POLARITY_HIGH;
        //在这里将全局变量设置为主机模式，方便后面的代码用主机模式的函数
        spi_deploy.spi_role = MY_SPI_MASTER;
        CS_Switch_To_Output();
    }else if (spi_mode == MY_SPI_SLAVE) {       //如果选择从机模式
        hspi6.Init.Mode = SPI_MODE_SLAVE;
        hspi6.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
        spi_deploy.spi_role = MY_SPI_SLAVE;

        CS_Switch_To_Exit();
    }
    //重新传入句柄，完成配置
    HAL_SPI_Init(&hspi6);
    if (spi_deploy.spi_role == MY_SPI_SLAVE) {
        //预开启接收中断
        HAL_SPI_TransmitReceive_DMA(&hspi6, tx_spi_buffer, rx_spi_buffer, DMA_BUFFER_LEN);
    }
}

/*****************************************主机模式***************************************/



/**
*   @brief 当被配置为主机模式的时候，使用这个函数发送数据,只负责发送
*   @param data:传入要发送的数据
*   @param len:要发送的数据长度
*/
void My_SPI_Send(uint8_t *data, uint32_t len){
    if (spi_deploy.spi_role == MY_SPI_MASTER) {
        if (if_busy == 0) {
            if_busy = 1;
            if (spi_deploy.cs_polarity == 0) CS_Pin_State(0);
            else CS_Pin_State(1);
            HAL_SPI_Transmit_IT(&hspi6, data, len);
        }
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi){
    if (hspi->Instance == hspi6.Instance) {
        if (spi_deploy.cs_polarity == 0) CS_Pin_State(1);
        else CS_Pin_State(0);
        if_busy = 0;
    }
}

/**
*   @brief 作为主机的时候，在发送数据的同时捕获MISO上的数据，发送的同时接收
*   @param tx_data:要发送的数据
*   @param rx_data:用来接收数据的缓冲区
*   @param len:数据长度
*/
void My_SPI_SendReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t len){
    if (spi_deploy.spi_role == MY_SPI_MASTER) {
        if (if_busy == 0) {
            if_busy = 1;
            if (spi_deploy.cs_polarity == 0) CS_Pin_State(0);
            else CS_Pin_State(1);
            HAL_SPI_TransmitReceive_IT(&hspi6, tx_data, rx_data, len);
        }
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
    if (hspi == &hspi6) {
        //当作为主机的时候
        if (spi_deploy.spi_role == MY_SPI_MASTER) {
            if (spi_deploy.cs_polarity == 0) CS_Pin_State(1);
            else CS_Pin_State(0);
            if_busy = 0;
        }else if(spi_deploy.spi_role == MY_SPI_SLAVE){         //作为从机的时候
            //接收完一段DMA_BUFFER_LEN长的数据之后溢出，重新开启接收中断
            SPI_PutData_To_Buffer();
            HAL_SPI_TransmitReceive_DMA(&hspi6, tx_spi_buffer, rx_spi_buffer, DMA_BUFFER_LEN);
        }
    }
}

/*****************************************从机模式**************************************/

/**
*   @brief 通过判断if_rx_finish标志位来判断是否完成一次接收，并将数据放入环形缓冲区
***/
uint8_t SPI_PutData_To_Buffer(){
    if (if_rx_finish) {
        //当一次接收完成
        uint32_t this_data_len = (DMA_BUFFER_LEN - __HAL_DMA_GET_COUNTER(&hdma_spi6_rx));       //数据长度
        uint32_t data_index = 0;                                
        for (data_index = 0; data_index < this_data_len; data_index++) {
            SPI_RangeBuffer_Wirte(rx_spi_buffer[data_index]);
        }
        if_rx_finish = 0;               //表示现在数据已经放入环形缓冲区里面了
        return 1;
    }
    return 0;
}


//当配置为从机的时候，在外部中断里面接收片选信号并开始接收数据
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
    if (GPIO_Pin == GPIO_PIN_10) {
        if (spi_deploy.spi_role == MY_SPI_SLAVE) {
            //在收到片选信号之后开启spi外设
            if (HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_10) == GPIO_PIN_RESET) {
                analyse_index++;
                spi_analyse[analyse_index].cs_low_tick = Get_Sys_us();         //记录cs引脚拉低的时刻
                
            }else if(HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_10) == GPIO_PIN_SET){ 
                //cs脉冲间隔
                spi_analyse[analyse_index].cs_hight_tick = Get_Sys_us();
                SPI_DMAStop_Manual(&hspi6);        //在结束片选之后关闭接收
                
                //统计总帧数、成功失败帧数  
                spi_analyse[analyse_index].spi_total_frame = analyse_index;
                if (spi_deploy.spi_error.error_code == 0) {
                    spi_analyse[0].spi_success_frame++;
                    spi_analyse[analyse_index].spi_success_frame = spi_analyse[0].spi_success_frame;
                }else {
                    spi_analyse[0].spi_fail_frame++;
                    spi_analyse[analyse_index].spi_fail_frame = spi_analyse[0].spi_fail_frame;
                }
                //计算成功率
                spi_analyse[analyse_index].transmit_success_rate = ((float_t)spi_analyse[analyse_index].spi_success_frame/spi_analyse[analyse_index].spi_total_frame);

                //计算帧间隔
                if (spi_analyse[analyse_index].spi_total_frame > 1) {
                    spi_analyse[analyse_index].spi_frame_gap = spi_analyse[analyse_index].cs_low_tick - spi_analyse[analyse_index-1].cs_hight_tick;
                }else {
                    spi_analyse[analyse_index].spi_frame_gap = 0;
                }
                //cs脉冲间隔
                spi_analyse[analyse_index].cs_gap_tick = spi_analyse[analyse_index].cs_hight_tick - spi_analyse[analyse_index].cs_low_tick; 
                /**********在这里可以做将数据放入环形缓冲区的操作*********/
                if_rx_finish = 1;            //在这里将标志位置1之后可以调用SPI_PutData_To_Buffer()将数据放入缓冲区       
                SPI_PutData_To_Buffer();
                
                /******************************************************/
                //开启下一次通信
                HAL_SPI_TransmitReceive_DMA(&hspi6, tx_spi_buffer, rx_spi_buffer, DMA_BUFFER_LEN);
            }
        }
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_10);
    }
}

/**************************************错误检测和协议分析************************************/
//后续有什么操作可以加进来
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi){
    //如果是spi6进入中断
    if (hspi == &hspi6) {
        spi_deploy.spi_error.error_code = hspi->ErrorCode;
        //模式错误
        if (spi_deploy.spi_error.error_code & HAL_SPI_ERROR_MODF) {
            spi_deploy.spi_error.mode_error++;
        }
        if (spi_deploy.spi_error.error_code & HAL_SPI_ERROR_OVR) {
            spi_deploy.spi_error.over_error++;
        }
        if (spi_deploy.spi_error.error_code & HAL_SPI_ERROR_DMA) {
            spi_deploy.spi_error.DMA_error++;
        }
        if (spi_deploy.spi_error.error_code & HAL_SPI_ERROR_CRC) {
            spi_deploy.spi_error.crc_error++;
        }
    }
}


/*********************************这个部分为接入RTOS***********************************/
/**
*   @brief 这个函数放在MX_FREERTOS_Init里面，用来创建所有的spi任务
*
****/

void My_SPI_Init_Task(void *argument){
    My_SPI_Init();
    while(1){

    }
}

void My_SPI_Task(void){
    //创建初始化任务
    xTaskCreate(My_SPI_Init_Task, "My_SPI_Init_Task", 128, NULL, osPriorityNormal, NULL);
}

