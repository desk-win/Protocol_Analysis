/*************************************************
 * DMA_Catch：TIM1 触发 DMA1，10MHz 采样 GPIOG->IDR
 *
 * DMA 双缓冲直接填共享内存（D2 SRAM1 @ SHM_DMA_CATCH_ADDR）：
 *   [0     .. 9999]  前半（HT 中断时填满）
 *   [10000 .. 19999]  后半（TC 中断时填满）
 *
 * ISR 只做标志（batch_ready）+ 累加计数 → CM7 直接读 DMA_CATCH_BUF 画波形。
 * 启停由 CM7 通过 SHM_CONFIG->dma_catch_enable 控制。
 *************************************************/

#include "my_dma_catch.h"
#include "main.h"
#include "shared_buf.h"      /* shm_init / __DMB */
#include <stdint.h>

#define HALF_SIZE  SHM_DMA_CATCH_HALF        /* 10000 条 */
#define FULL_SIZE  SHM_DMA_CATCH_SIZE        /* 20000 条 */

/* 用于兼容 my_gpio_check 的环形缓冲读指针（SPI 模块可能用）*/
static volatile uint16_t dma_read_head = 0;

SemaphoreHandle_t DMA_Catch_Semaphore = NULL;

/*==============================================================================
 * 初始化：DMA CIRCULAR 模式，直接填共享缓冲区
 *============================================================================*/
void My_DMA_Catch_Init(void)
{
    /* 1. 创建信号量（初始 0，ISR 中 Give）*/
    DMA_Catch_Semaphore = xSemaphoreCreateBinary();

    /* 2. 重新配置 DMA 为 CIRCULAR 模式 */
    hdma_tim1_up.Init.Mode = DMA_CIRCULAR;
    HAL_DMA_Init(&hdma_tim1_up);

    /* 3. 注册 HT / TC 回调 */
    HAL_DMA_RegisterCallback(&hdma_tim1_up, HAL_DMA_XFER_HALFCPLT_CB_ID,
                             HAL_DMA_XferHalfCpltCallback);
    HAL_DMA_RegisterCallback(&hdma_tim1_up, HAL_DMA_XFER_CPLT_CB_ID,
                             HAL_DMA_XferCpltCallback);

    /* 4. 使能 HT / TC 中断 */
    __HAL_DMA_ENABLE_IT(&hdma_tim1_up, DMA_IT_HT);
    __HAL_DMA_ENABLE_IT(&hdma_tim1_up, DMA_IT_TC);

    /* 5. 清零共享控制块 */
    DMA_CATCH_CTRL->batch_ready  = 0;
    DMA_CATCH_CTRL->sample_count = 0;
    dma_read_head = 0;
    __DMB();

    /* 6. DMA 直接写到共享地址 DMA_CATCH_BUF */
    HAL_DMA_Start(&hdma_tim1_up, (uint32_t)&GPIOG->IDR,
                  (uint32_t)DMA_CATCH_BUF, FULL_SIZE);

    /* 7. 启动定时器触发 DMA */
    __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);
    HAL_TIM_Base_Start(&htim1);
}

/*==============================================================================
 * 停止
 *============================================================================*/
void My_DMA_Catch_Stop(void)
{
    HAL_TIM_Base_Stop(&htim1);
    __HAL_TIM_DISABLE_DMA(&htim1, TIM_DMA_UPDATE);
    HAL_DMA_Abort(&hdma_tim1_up);
    DMA_CATCH_CTRL->batch_ready  = 0;
    DMA_CATCH_CTRL->sample_count = 0;
    __DMB();
}

/*==============================================================================
 * DMA 半传输完成（HT）— 前半段 [0 .. 9999] 填满
 *   由 DMA1_Stream3_IRQHandler → HAL_DMA_IRQHandler 内部调用，ISR 上下文
 *============================================================================*/
void HAL_DMA_XferHalfCpltCallback(DMA_HandleTypeDef *hdma)
{
    if (hdma->Instance != DMA1_Stream3) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    DMA_CATCH_CTRL->batch_ready |= 0x01;
    DMA_CATCH_CTRL->sample_count += HALF_SIZE;
    __DMB();

    if (DMA_Catch_Semaphore != NULL) {
        xSemaphoreGiveFromISR(DMA_Catch_Semaphore, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*==============================================================================
 * DMA 传输完成（TC）— 后半段 [10000 .. 19999] 填满
 *============================================================================*/
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
    if (hdma->Instance != DMA1_Stream3) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    DMA_CATCH_CTRL->batch_ready |= 0x02;
    DMA_CATCH_CTRL->sample_count += HALF_SIZE;
    __DMB();

    if (DMA_Catch_Semaphore != NULL) {
        xSemaphoreGiveFromISR(DMA_Catch_Semaphore, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*==============================================================================
 * GPIO 快照读取（保留兼容，供 SPI 模块用 my_gpio_check）
 *============================================================================*/
#define GET_PIN(snap, pin) (((snap) >> (pin)) & 0x01)

GPIO_State my_gpio_check(void)
{
    static GPIO_State my_gpio_state;

    /* 用 DMA NDTR 计数器推算出当前写指针 */
    uint16_t tail = (uint16_t)(FULL_SIZE - __HAL_DMA_GET_COUNTER(&hdma_tim1_up));
    if (tail >= FULL_SIZE) tail = 0;

    /* 读当前 head 位置的一个采样 */
    my_gpio_state.PG7_State  = GET_PIN(DMA_CATCH_BUF[dma_read_head], 7);
    my_gpio_state.PG10_State = GET_PIN(DMA_CATCH_BUF[dma_read_head], 10);
    my_gpio_state.PG12_State = GET_PIN(DMA_CATCH_BUF[dma_read_head], 12);

    /* 推进读指针（不追尾）*/
    if ((dma_read_head != tail) && ((tail + 1U) % FULL_SIZE != dma_read_head)) {
        dma_read_head = (uint16_t)((dma_read_head + 1U) % FULL_SIZE);
    }

    return my_gpio_state;
}

/*==============================================================================
 * RTOS 控制任务：轮询 SHM_CONFIG->dma_catch_enable
 *   0=停止, 1=开启。CM7 通过 Settings Apply 切换。
 *============================================================================*/
void DMA_Catch_Ctrl_Task(void *argument)
{
    uint8_t running = 0;

    while (1) {
        uint8_t enable = SHM_CONFIG->dma_catch_enable;

        if (enable && !running) {
            My_DMA_Catch_Init();
            running = 1;
        }
        else if (!enable && running) {
            My_DMA_Catch_Stop();
            running = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
