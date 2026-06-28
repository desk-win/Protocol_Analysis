/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "my_uart_check.h"
#include "my_can_check.h"
#include "my_i2c_check.h"
#include "my_spi_check.h"
#include "shared_config.h"
#include "usart.h"
#include "shared_buf.h"
#include "shared_buf.h"
#include "my_dma_catch.h"
#include "adc_cpld.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_types.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
volatile uint8_t g_config_pending = 0;   /* HSEM ID=1 中断置位 → main 循环读 SHM_CONFIG 重配 UART */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void apply_uart_config_from_shm(void);
static void apply_can_config_from_shm(void);
static void apply_i2c_config_from_shm(void);
static void apply_spi_config_from_shm(void);


void Proto_Select(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  xTaskCreate(Proto_Select, "Proto_Select", 512, NULL, osPriorityAboveNormal, NULL);

  /* 数据采集控制任务（轮询 SHM_CONFIG->dcmi_enable / dma_catch_enable）*/
  xTaskCreate(Control_DCMI_Task, "DCMI_Ctrl", 128, NULL, osPriorityLow, NULL);
  xTaskCreate(DMA_Catch_Ctrl_Task, "DMA_Ctrl", 128, NULL, osPriorityLow, NULL);

  
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* CM4 持续抓 UART → 共享内存。
   * 注：自环回发送 My_UART_Send_Single 的 while(gState==BUSY_TX) 会因 TxCplt 中断
   * 未把 gState 设回 READY 而死循环，导致 task 卡死(只发一次)。这里去掉自发送，
   * 改为持续接收 + 每秒心跳字节，验证 CM4 task 持续跑 + 共享内存持续传输。 */
  uart1_printf("\r\n[CM4] UART grab -> shared memory (heartbeat every second)\r\n");
  uint8_t rx_buf[64];
  uint32_t tick = 0;
  /* Infinite loop */
  for(;;)
  {
    /* 1. 持续读 UART 环形缓冲（非阻塞），抓到的字节写入共享内存 */
    uint32_t n = My_UART_Read_RingBuffer(rx_buf, sizeof(rx_buf));
    for (uint32_t i = 0; i < n; i++) {
      shm_push(rx_buf[i]);
    }
    /* 2. 心跳：每秒(100*10ms)写一个递增字节。暂去掉 alive printf——uwTick 在
     * printf 时被破坏(栈增大无效)，隔离 printf 看 CM7 cnt 是否能持续涨。*/
    if (tick % 100 == 0)
    {
      uint8_t hb = (uint8_t)('A' + (tick / 100U) % 26U);
      shm_push(hb);
    }

    tick++;
    HAL_Delay(10);  /* SysTick 中断没触发(osDelay 卡)，临时用 HAL_Delay(TIM7 tick) */
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* M4创建的第一个任务，在循环里面不断等待M7的配置数据 */
void Proto_Select(void *argument){
  for(;;)
  {
    /* HSEM 通知：CM7 改了协议配置 → 读 SHM_CONFIG 重配对应外设 */
    if (g_config_pending) {
      g_config_pending = 0;
      if (SHM_CONFIG->active_proto == 1) {
        uart_rxcallback_semaphore = xSemaphoreCreateBinary();
        uart_txcallback_semaphore = xSemaphoreCreateBinary();
        apply_uart_config_from_shm();
        xTaskCreate(UART_Callback_Task, "UART_Callback_Task", 256, NULL, osPriorityNormal, NULL);
      }
      else if (SHM_CONFIG->active_proto == 2) {
        spi_slavecallback_semaphore = xSemaphoreCreateBinary();
        spi_mastercallback_semaphore = xSemaphoreCreateBinary();
        apply_spi_config_from_shm();
        xTaskCreate(SPI_Callback_Task, "SPI_Callback_Task", 256, NULL, osPriorityNormal, NULL);
      }
      else if (SHM_CONFIG->active_proto == 3) {
        i2c_mastercallback_semaphore = xSemaphoreCreateBinary();
        i2c_slavecallback_semaphore = xSemaphoreCreateBinary();
        apply_i2c_config_from_shm();
        xTaskCreate(I2C_Callback_Task, "I2C_Callback_Task", 256, NULL, osPriorityNormal, NULL);
      }
      else if (SHM_CONFIG->active_proto == 4) {
        can_rxcallback_semaphore = xSemaphoreCreateBinary();
        can_txcallback_semaphore = xSemaphoreCreateBinary();
        apply_can_config_from_shm();
        xTaskCreate(CAN_Callback_Task, "CAN_Callback_Task", 256, NULL, osPriorityNormal, NULL);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


/* 读 SHM_CONFIG->uart 映射 HAL 枚举 → UART_Param_Change 重配 USART6。
 * STM32 USART 硬件限制：databits 只支持 7/8/9（5/6→8B），stopbits 只 1/2（1.5→2）。*/
static void apply_uart_config_from_shm(void)
{
  uart_config_t *u = &SHM_CONFIG->uart;

  /* parity：0=None, 1=Even, 2=Odd（先算，wordlen 依赖它）*/
  uint32_t par = UART_PARITY_NONE;
  if (u->parity == 1) par = UART_PARITY_EVEN;
  else if (u->parity == 2) par = UART_PARITY_ODD;

  /* databits → WordLength。STM32 WordLength 含 parity bit（PCE=1 时 MSB 是校验位，
   * 数据位 = WordLength-1）。带 parity 时 WordLength 升一档：8 data + Even → 9B。
   * 5/6 退化 8B（硬件不支持）；9 + parity 需 10B 不支持，保持 9B（实际 8 data + parity）。*/
  uint32_t wordlen = UART_WORDLENGTH_8B;
  if (u->databits == 7) wordlen = UART_WORDLENGTH_7B;
  else if (u->databits == 9) wordlen = UART_WORDLENGTH_9B;
  if (par != UART_PARITY_NONE) {
    if (wordlen == UART_WORDLENGTH_7B) wordlen = UART_WORDLENGTH_8B;
    else if (wordlen == UART_WORDLENGTH_8B) wordlen = UART_WORDLENGTH_9B;
  }

  /* stopbits：1/2，1.5 退化 2（常规 USART 不支持 1.5）*/
  uint32_t stopb = (u->stopbits >= 2) ? UART_STOPBITS_2 : UART_STOPBITS_1;

  /* flowcontrol：0=None, 1=RTS, 2=CTS, 3=RTS_CTS */
  uint32_t flow = UART_HWCONTROL_NONE;
  if (u->flowcontrol == 1) flow = UART_HWCONTROL_RTS;
  else if (u->flowcontrol == 2) flow = UART_HWCONTROL_CTS;
  else if (u->flowcontrol == 3) flow = UART_HWCONTROL_RTS_CTS;

  if (UART_Param_Change(u->baudrate, wordlen, stopb, par, flow) == HAL_OK) {
    uart1_printf("[CM4] UART reconfig OK: %lu baud, dbit=%u stop=%u par=%u flow=%u\r\n",
                 (unsigned long)u->baudrate, (unsigned)u->databits,
                 (unsigned)u->stopbits, (unsigned)u->parity, (unsigned)u->flowcontrol);
  } else {
    uart1_printf("[CM4] UART reconfig FAIL\r\n");
  }
}

/* 读 SHM_CONFIG->can → 算 BRP/BS1/BS2（采样点 87.5%）+ mode 映射 → CAN_Param_Change。
 * FDCAN kernel clock = PLL（PLLQ），假设 240MHz（待实测确认，错则波特率不对）。
 * 公式 Baud = FDCAN_CLK / (BRP * (1+BS1+BS2))，固定 BS1=13/BS2=2/SJW=1（16 tq，采样点 87.5%）。*/
/* 读 SHM_CONFIG->can → 波特率 + 模式 + TX 报文格式 + RX 过滤器 */
static void apply_can_config_from_shm(void)
{
  can_config_t *c = &SHM_CONFIG->can;

  /* ── 1. 波特率和模式 ── */
  uint32_t mode = FDCAN_MODE_NORMAL;
  if (c->mode == 1) mode = FDCAN_MODE_EXTERNAL_LOOPBACK;
  else if (c->mode == 2) mode = FDCAN_MODE_BUS_MONITORING;
  else if (c->mode == 3) mode = FDCAN_MODE_INTERNAL_LOOPBACK;

  #define FDCAN_KERNEL_CLK  120000000U   /* PLL1Q = 960MHz / PLLQ(8) = 120MHz */
  uint32_t brp = FDCAN_KERNEL_CLK / (c->baudrate * 16);
  if (brp < 1) brp = 1;
  if (brp > 512) brp = 512;

  if (CAN_Param_Change(c->baudrate, brp, 13, 2, 1, mode, ENABLE) != HAL_OK) {
    uart1_printf("[CM4] CAN reconfig FAIL\r\n");
    return;
  }

  /* ── 2. TX 报文格式（发送前初始化）── */
  Can_ID_Type   id_type   = (c->tx_id_type == 1)    ? CAN_ID_EXTENDED : CAN_ID_STANDARD;
  Can_Frame_Type frame_type = (c->tx_frame_type == 1) ? CAN_FRAME_REMOTE : CAN_FRAME_DATA;
  My_CAN_Send_Single_Init(c->tx_id, id_type, frame_type, c->tx_dlc);

  /* ── 3. RX 过滤器 ── */
  Can_ID_Type filt_id_type = (c->filter_id_type == 1) ? CAN_ID_EXTENDED : CAN_ID_STANDARD;
  CAN_Filter_Config(c->filter_mode, c->filter_id, filt_id_type, c->filter_fifo);

  uart1_printf("[CM4] CAN reconfig OK: %lu baud, TX id=0x%lX %s, filter=%s\r\n",
               (unsigned long)c->baudrate,
               (unsigned long)c->tx_id,
               (c->tx_id_type ? "EXT" : "STD"),
               (c->filter_mode ? "MATCH" : "ALL"));
}

/* 读 SHM_CONFIG->i2c → My_I2C_Init 重配 I2C4。
 * own_mode: 0=从机（监听 own_address），1=主机（向 own_address 发起通信）
 * clock_speed: 100000(标准) / 400000(快速) / 1000000(快速+)
 * addressing: 0=7-bit, 1=10-bit
 * own_address: 从机时=自身地址，主机时=目标从机地址 */
static void apply_i2c_config_from_shm(void)
{
  i2c_config_t *i = &SHM_CONFIG->i2c;
  uint8_t  addr_mode  = (i->addressing == 1) ? 10 : 7;
  I2C_Mode mode       = (i->own_mode == 1) ? MY_I2C_MASTER : MY_I2C_SLAVE;
  uint8_t  addr_7bit  = (addr_mode == 7)  ? (uint8_t)i->own_address : 0;
  uint16_t addr_10bit = (addr_mode == 10) ? i->own_address : 0;

  My_I2C_Init(mode, addr_mode, addr_7bit, addr_10bit, 0, i->clock_speed);

  uart1_printf("[CM4] I2C reconfig OK: %s, %lu Hz, %u-bit, addr=0x%04X\r\n",
               (mode == MY_I2C_MASTER) ? "MASTER" : "SLAVE",
               (unsigned long)i->clock_speed,
               (unsigned)addr_mode,
               (unsigned)i->own_address);
}

/* 读 SHM_CONFIG->spi → My_SPI_Init 重配 SPI6。
 * role: 0=Slave（被动收，baudrate 无效），1=Master（主动收发）。
 * datasize 在 init 前赋 hspi6.Init.DataSize（队友 My_SPI_Init 不覆盖该字段，HAL_SPI_DeInit 也不清 Init 结构体）。
 * cs_polarity v1 硬编码 0（低有效），UI 未暴露。*/
static void apply_spi_config_from_shm(void)
{
  spi_config_t *s = &SHM_CONFIG->spi;
  /* prescaler: master 按 baud 算，slave 不用（presc=0 标 N/A，slave 跟主机时钟）*/
  uint16_t presc = (s->role == 1) ? spi_prescaler_from_baud(s->baudrate) : 0;
  hspi6.Init.DataSize = spi_datasize_from_u8(s->datasize);
  hspi6.Init.FirstBit = (s->firstbit) ? SPI_FIRSTBIT_LSB : SPI_FIRSTBIT_MSB;
  if (s->role == 1) {
    My_SPI_Init(s->mode, MY_SPI_MASTER, 0, presc, 0);
  } else {
    My_SPI_Init(s->mode, MY_SPI_SLAVE, 0, 64, 0);
  }
  uart1_printf("[CM4] SPI reconfig OK: role=%s mode=%u ds=%u first=%u baud=%lu presc=%u\r\n",
               s->role ? "MASTER" : "SLAVE", (unsigned)s->mode,
               (unsigned)s->datasize, (unsigned)s->firstbit,
               (unsigned long)s->baudrate, (unsigned)presc);
}


/* HSEM Free Callback：CM7 Release HSEM_ID_CONFIG 时触发（通知 config 就绪）。
 * 不在中断里直接重配 UART（避免与 main 循环的 UART 发送/DMA 竞态），只置 flag。*/
void HAL_HSEM_FreeCallback(uint32_t SemMask)
{
  if (SemMask & __HAL_HSEM_SEMID_TO_MASK(HSEM_ID_CONFIG))
  {
    g_config_pending = 1;
    /* 重新激活通知：确保下次 CM7 Release 仍触发中断（防御性，部分场景 IER 被清）*/
    HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_CONFIG));
  }
}
/* USER CODE END Application */

