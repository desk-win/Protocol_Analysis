#include "my_dwt_count.h"
#include "stm32h747xx.h"
#include "system_stm32h7xx.h"
#include <stdint.h>



/**
*   @brief 使用DWT的CYCCNT需要初始化
**/
void My_DWT_Init(){
    //解锁DWT访问
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    //清零周期计数器
    DWT->CYCCNT = 0;
    //开启计数器
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
*   @brief 获取当前系统微秒数
**/
uint32_t Get_Sys_us(){
    return DWT->CYCCNT / (SystemCoreClock/1000000);
}


