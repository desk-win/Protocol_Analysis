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
#include "bdma.h"
#include "dcmi.h"
#include "dma.h"
#include "fdcan.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include "usart_printf.h"
#include "adc_cpld.h"
#include "usart.h"
#include "my_uart_check.h"
#include "my_can_check.h"
#include "my_i2c_check.h"
#include "my_spi_check.h"
#include "my_dwt_count.h"
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


/* SPI master 模式 IT 非阻塞：tx/rx buffer 必须 file-scope static（不能用栈）*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MPU_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

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
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_BDMA_Init();
  MX_SPI6_Init();
  MX_USART6_UART_Init();
  MX_I2C4_Init();
  MX_DCMI_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  MX_FDCAN1_Init();
  /* USER CODE BEGIN 2 */
  uart1_printf("\r\n[CM4] enter USER CODE 2 (boot sync 已过)\r\n");
  My_UART_Init();   /* 启动 USART6 DMA + IDLE 空闲中断接收 */

  /* CAN 初始化：过滤器全通（标准帧→FIFO0）+ 启动 + 激活接收/错误中断。
   * HAL_FDCAN_RxFifo0Callback（在 my_can_check.c）自动收帧入 my_can_rangebuffer。*/
  CAN_Filter_Config(0, 0, CAN_ID_STANDARD, 0);
  HAL_FDCAN_Start(&hfdcan1);
  CAN_Handler_Start();

  /* DWT 初始化（I2C 协议分析用 Get_Sys_us 测时钟拉伸/帧间隔）*/
  My_DWT_Init();
  /* SHM_CONFIG->i2c.own_address 默认值初始化（UI 不暴露此字段，固定 0x50）。
   * 不初始化的话首字节是 SRAM 残留垃圾，首次 Apply 会把垃圾地址传给 My_I2C_Init。*/
  SHM_CONFIG->i2c.own_address = I2C_DEFAULT_OWNADDR;
  /* I2C 初始化：默认从机模式（own_address=0x50），7bit 寻址，100kHz。
   * HAL_I2C_AddrCallback（在 my_i2c_check.c）自动收帧入 my_i2c_data。*/
  My_I2C_Init(MY_I2C_SLAVE, 7, 0x50, 0, 0, 100000);

  /* SPI 初始化：默认从机模式（mode0/CS低有效），CS 当 EXTI 双边沿 + DMA 预 arm 收数据。
   * HAL_GPIO_EXTI_Callback（在 my_spi_check.c）自动收帧入 spi_range_buffer。*/
  HAL_GPIO_WritePin(SPI6_CS_GPIO_Port, SPI6_CS_Pin, GPIO_PIN_SET);  /* CS regen 兜底：gpio.c:49 regen 重置成 RESET */
  My_SPI_Init(0, MY_SPI_SLAVE, 0, 64, 0);

  /* 激活 HSEM ID=1 通知：CM7 写完 SHM_CONFIG 后 Release → 中断置 g_config_pending → 本循环重配外设。
   * 放外设初始化之后：确保 UART6/FDCAN1/I2C4 初始化完成才接收重配请求。*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_CONFIG));

  /* CM4 初始化 ring buffer：CM4→CM7 方向通(CM4无cache,写直达物理)，
   * CM7→CM4 方向不通(CM7写停在D-Cache,clean也flush不到CM4)，故 shm_init 由 CM4 调用。*/
  shm_init();
  uart1_printf("[CM4] shm_init done: head/tail=0\r\n");
  
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
