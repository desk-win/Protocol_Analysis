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
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_touchgfx.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* TouchGFX task: static allocation with stack in AXI SRAM.
 * 48KB stack is placed in AXI SRAM via xTaskCreateStatic to avoid
 * consuming DTCM and to sidestep SDRAM bus contention issues. */
#define TOUCHGFX_STACK_WORDS  12288  /* 48 KB */
static StaticTask_t touchgfxTCB;
__attribute__((section(".axi_stack"))) static StackType_t touchgfxStack[TOUCHGFX_STACK_WORDS];
osThreadId_t touchgfxTaskHandle;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void StartTouchGFXTask(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize);

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  argument: Not used
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN RTOS_THREADS */
  /* Create TouchGFX task with STATIC allocation (stack in AXI SRAM) */
  touchgfxTaskHandle = xTaskCreateStatic(
      StartTouchGFXTask,
      "TouchGFX",
      TOUCHGFX_STACK_WORDS,
      NULL,
      osPriorityAboveNormal,
      touchgfxStack,
      &touchgfxTCB);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
}

/* USER CODE BEGIN Application */
/**
  * @brief TouchGFX task entry point.
  *        Initializes TouchGFX and enters the main event loop.
  *        This function never returns.
  */
void StartTouchGFXTask(void *argument)
{
    MX_TouchGFX_Init();
    /* Ensure backlight stays ON — TouchGFX handles the display */
    HAL_GPIO_WritePin(BL_CTR_GPIO_Port, BL_CTR_Pin, GPIO_PIN_SET);
    MX_TouchGFX_Process();  /* enters event loop, never returns */
}

/**
  * @brief Default task — empty loop.
  *        TouchGFX manages the framebuffer; no bare-metal drawing here.
  *        (Overridden via USER CODE section so CubeMX regen won't overwrite)
  */
void StartDefaultTask(void *argument)
{
    for (;;)
    {
        osDelay(1000);
    }
}
/* USER CODE END Application */
