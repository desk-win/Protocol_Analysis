/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "dma.h"
#include "fdcan.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "my_uart_check.h"
#include "shared_buf.h"
#include "shared_config.h"   /* HSEM_ID_CONFIG + SHM_CONFIG 协议配置（CM7 写 → CM4 读改外设）*/
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
/* To comment when developping/debugging on a single core                     */
#define DUAL_CORE_BOOT_SYNC_SEQUENCE  /* 恢复：阶段2 OPENAMP 需要双核 boot 同步 */

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t g_config_pending = 0;   /* HSEM ID=1 中断置位 → main 循环读 SHM_CONFIG 重配 UART */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MPU_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
static void apply_uart_config_from_shm(void);   /* HSEM 通知后：读 SHM_CONFIG 映射 HAL 枚举 → UART_Param_Change */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /* Activate HSEM notification for Cortex-M4*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
  /*
  Domain D2 goes to STOP mode (Cortex-M4 in deep-sleep) waiting for Cortex-M7 to
  perform system initialization (system clock config, external memory configuration.. )
  */
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
  /* Clear HSEM flag */
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_DMA_Init();
  MX_USART6_UART_Init();
  MX_FDCAN1_Init();
  MX_I2C4_Init();
  MX_SPI6_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  uart1_printf("\r\n[CM4] enter USER CODE 2 (boot sync 已过)\r\n");
  My_UART_Init();   /* 启动 USART6 DMA + IDLE 空闲中断接收 */

  /* 激活 HSEM ID=1 通知：CM7 写完 SHM_CONFIG 后 Release → 中断置 g_config_pending → 本循环重配 UART。
   * 放 My_UART_Init 之后：确保 UART6 初始化完成才接收重配请求。*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_CONFIG));

  /* CM4 初始化 ring buffer：CM4→CM7 方向通(CM4无cache,写直达物理)，
   * CM7→CM4 方向不通(CM7写停在D-Cache,clean也flush不到CM4)，故 shm_init 由 CM4 调用。*/
  shm_init();
  uart1_printf("[CM4] shm_init done: head/tail=0\r\n");

  /* 裸机 main 循环：UART6 抓取 → 共享内存（给 CM7）。osKernelStart 永不执行。*/
  uart1_printf("[CM4] bare-metal: UART6 grab -> shm (PG14<->PG9 self-loop)\r\n");
  uint8_t rx_buf[64];
  uint8_t tx_buf[5];
  uint8_t tx_base = 0;
  uint32_t bm_tick = 0;
  for(;;)
  {
    /* HSEM 通知：CM7 改了协议配置 → 读 SHM_CONFIG 重配 USART6（仅 active_proto==1=UART）*/
    if (g_config_pending) {
      g_config_pending = 0;
      if (SHM_CONFIG->active_proto == 1) apply_uart_config_from_shm();
    }
    /* 每秒发 5 个递增字节（0,1,2,3,4 / 5,6,7,8,9 ...），每个字节 bit 图案不同，
     * 波形持续滚动变化（Hello 重复会波形不变）。PG14 TX → PG9 RX 自环回。*/
    if (bm_tick % 100 == 0)
    {
      for (int i = 0; i < 5; i++) tx_buf[i] = (uint8_t)(tx_base + i);
      HAL_UART_Transmit(&huart6, tx_buf, 5, 100);
      tx_base += 5;
    }
    /* 读 UART 环形缓冲，抓到的字节写入共享内存给 CM7 */
    uint32_t n = My_UART_Read_RingBuffer(rx_buf, sizeof(rx_buf));
    for (uint32_t i = 0; i < n; i++) shm_push(rx_buf[i]);
    bm_tick++;
    HAL_Delay(10);
  }
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */
/* HSEM Free Callback：CM7 Release HSEM_ID_CONFIG 时触发（通知 config 就绪）。
 * 不在中断里直接重配 UART（避免与 main 循环的 UART 发送/DMA 竞态），只置 flag。*/
void HAL_HSEM_FreeCallback(uint32_t SemMask)
{
  if (SemMask & __HAL_HSEM_SEMID_TO_MASK(HSEM_ID_CONFIG))
  {
    g_config_pending = 1;
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
    uart1_printf("[CM4] UART reconfig OK: %lu baud, dbit=%u stop=%u par=%u\r\n",
                 (unsigned long)u->baudrate, (unsigned)u->databits,
                 (unsigned)u->stopbits, (unsigned)u->parity);
  } else {
    uart1_printf("[CM4] UART reconfig FAIL\r\n");
  }
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* SRAM4 (D3 domain 0x38000000) 共享内存：shareable + non-cacheable（双核通信） */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x38000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64KB;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* AXI SRAM 共享内存段（0x24060000）：shareable + non-cacheable */
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.BaseAddress = 0x2407C000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16KB;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM7 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
