#include "adc_cpld.h"
#include "dcmi.h"
#include "main.h"
#include "projdefs.h"
#include "stm32h747xx.h"
#include "stm32h7xx_hal_dcmi.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_dma_ex.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_tim.h"
#include "tim.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "shared_buf.h"
#include "shared_config.h"


/**************************************************************
*   在调用DCMI_DoubleBuffer_Start()之后cpld复位，dcmi开始接收
*   调用调用adc_a_buf和adc_b_buf能获取两个ADC采集的数据
*
**************************************************************/

uint8_t dcmi_halfcount = 0,dcmi_count = 0,dcmi_it=0;
uint8_t dcmi_start=0,dma_en=0;

volatile uint8_t frame_flag = 0;
volatile uint32_t saved_ndtr = 0;
uint8_t *buf;
//定义一个大缓冲区
static uint32_t raw_big_buf[RAW_BUF_WORDS*2] __attribute__((aligned(32)));
//两个小缓冲区分别指向大缓冲区的开头和中间的地址
uint32_t *raw_buf_0 = &raw_big_buf[0]; 
uint32_t *raw_buf_1 = &raw_big_buf[RAW_BUF_WORDS];

// 外部调用adc_a_buf和adc_b_buf能获取两个ADC采集的数据

// uint8_t adc_a_buf[CHANNEL_SAMPLES];
// uint8_t adc_b_buf[CHANNEL_SAMPLES];
volatile bool frame_ready = false;
volatile uint16_t last_frame_id  = 0xFFFF;

/**
*   @brief 输出30mhz的时钟给CPLD芯片作为基准时钟
*   @param enable 1为开启输出，0为停止
**/
void CPLD_Base_Clock(uint8_t enable){
    if (enable) {
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    }else {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    }
}

/**
*   @brief 数据解析函数
*   每帧数据的帧头是AA、55、两个字节合起来是帧数、D0、前面三个字节相加
**/
void Parse_Frame(const uint8_t *raw){
    //遍历缓冲区找到帧头AA 55
    uint32_t header_pos = 0xFFFFFFFF;
    for (uint32_t i = 0; i < FRAME_TOTAL_BYTES - 1; i++) {
        //如果找到帧头
        if (raw[i] == HEADER_SYNC_AA && raw[i+1] == HEADER_SYNC_55) {
            if (i+HEADER_SIZE-1 >= FRAME_TOTAL_BYTES) break;    //如果这个帧头（6个字节）超过了帧总长度
            //没问题之后继续检验后续的四个字节
            uint8_t frame_lo = raw[i+2];
            uint8_t frame_hi = raw[i+3];
            uint8_t flag = raw[i+4];
            uint8_t checksum = raw[i+5];
            uint8_t expected = (uint8_t)(frame_lo+frame_hi+flag);

            if(flag == HEADER_FLAG && checksum == expected){
                header_pos = i;
                //检测丢帧,在cpld里面每发完一帧就将内部计数器加一并拆成字节2和3放在帧头，通过比较这个可以看出来有没有丢帧
                uint16_t frame_id = (uint16_t)(frame_hi << 8)|frame_lo;
                if (last_frame_id != 0xFFFF && (uint16_t)(frame_id - last_frame_id) != 1u) {
                    //这里表示判断为丢帧，可以进行相应操作
                }
                last_frame_id = frame_id;

                dcmi_start=1;

                break;
            }
        }
    }
    //表示没有找到帧头，返回
    if(header_pos == 0xFFFFFFFF) return;        

    //如果找到帧头了，开始处理数据
    //帧头占6个字节，从第7个字节开始就是ADC数据了，奇数是ADC_A,偶数是ADC_B
    uint32_t data_start = header_pos + HEADER_SIZE;
    uint32_t sample_index = 0;              //分成两个缓冲区，每个缓冲区的索引
    uint32_t pos = data_start;              //原来总缓冲区的索引
    while (pos+1 < FRAME_TOTAL_BYTES && sample_index < CHANNEL_SAMPLES) {
        DCMI_ADC_A[sample_index] = raw[pos];
        DCMI_ADC_B[sample_index] = raw[pos+1];
        sample_index++;
        pos += 2;
    }
    //通知已经将一个缓冲区里面的数据解析完成
      if (sample_index >= CHANNEL_SAMPLES) {
        __DMB();
        DCMI_SHM_CTRL->frame_ready = 1;
        DCMI_SHM_CTRL->frame_id++;
        __DMB();
    }
}



void My_DCMI_DMAHalfCallback(DMA_HandleTypeDef *hdma){

    Parse_Frame((const uint8_t *)raw_buf_0);
    
}

void My_DCMI_DMACpltCallback(DMA_HandleTypeDef *hdma){
    
    Parse_Frame((const uint8_t *)raw_buf_1);
}

/**
*   @brief CPLD复位函数
*
**/
void CPLD_Reset(){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
}

void CPLD_Stop(){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
}

/**
*   @brief 开启DMA双缓冲模式，利用DMA的半满回调，实现对两个缓冲区的数据交替处理
*
**/
void DCMI_DoubleBuffer_Start(){
    CPLD_Stop();
    CPLD_Base_Clock(1);
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)raw_big_buf, RAW_BUF_WORDS * 2);
    //注册半满和全满回调函数
    hdcmi.DMA_Handle->XferHalfCpltCallback = My_DCMI_DMAHalfCallback;
    hdcmi.DMA_Handle->XferCpltCallback = My_DCMI_DMACpltCallback;
    
    __HAL_DMA_ENABLE_IT(hdcmi.DMA_Handle, DMA_IT_HT);
    
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
}

/**
*   @brief 控制DCMI开启采集和停止
**/
void Control_DCMI_Task(void *argument){
    while (1) {
        //如果置1表示要开启dcmi
        if (SHM_CONFIG->dcmi_enable) {
            DCMI_DoubleBuffer_Start();
        }else {
            CPLD_Stop();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


