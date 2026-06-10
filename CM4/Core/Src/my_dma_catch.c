#include "my_dma_catch.h"
#include "my_spi_check.h"



/*************************************************
*   通过定时器和DMA读取PG7 PG10 PG12三个引脚上的状态
*   为了防止cpu和DMA读写速度不一致，引入了环形缓冲区
*
*************************************************/
#define RANGE_BUFFER_LEN    1000                    //环形缓冲区长度
#define GET_PIN(snap, pin) (((snap) >> (pin)) & 0x01)

uint16_t dma_gpio_buffer[1000];
range_buffer dma_range_buffer;                      //环形缓冲区读写指针结构体


/**
*   @brief 开启DMA和定时器代码，用来读取idr寄存器
*   @retval none
*/
void My_DMA_Catch_Init(void){
    HAL_DMA_Start(&hdma_tim1_up,(uint32_t)&GPIOG->IDR, (uint32_t)dma_gpio_buffer, RANGE_BUFFER_LEN);
    __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);
    HAL_TIM_Base_Start(&htim1);
    dma_range_buffer.len = RANGE_BUFFER_LEN;
}

/**
*   @brief 这个函数需要不断读取来获得三个引脚的状态
*   @retval GPIO_State:为三个引脚的句柄，包含了三个引脚的状态数据
*/
GPIO_State my_gpio_check(void){
    static GPIO_State my_gpio_state;
    //利用DMA缓冲区的计数器来得到当前可写入位置      
    dma_range_buffer.tail = 1000-__HAL_DMA_GET_COUNTER(&hdma_tim1_up);
    if (dma_range_buffer.tail >= 1000) dma_range_buffer.tail = 0;
    //当前可读取的位置
    //从dma_gpio_buffer中解算出三个引脚状态
    my_gpio_state.PG7_State = GET_PIN(dma_gpio_buffer[dma_range_buffer.head], 7);
    my_gpio_state.PG10_State = GET_PIN(dma_gpio_buffer[dma_range_buffer.head], 10);
    my_gpio_state.PG12_State = GET_PIN(dma_gpio_buffer[dma_range_buffer.head], 12);
    //当读指针不超过写指针且写指针不会超过读指针一圈的时候
    if((dma_range_buffer.head != dma_range_buffer.tail) && ((dma_range_buffer.tail+1)%dma_range_buffer.len != dma_range_buffer.head)){
        dma_range_buffer.head++;
    }
    if (dma_range_buffer.head >= 1000) {        //读指针超过了缓冲区总大小清零
        dma_range_buffer.head = 0;
    }
    return my_gpio_state;
}
