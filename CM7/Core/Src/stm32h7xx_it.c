/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ======== Fault diagnostics — SDRAM dump + backlight blink ======== */
#define FAULT_DUMP_ADDR  ((volatile uint32_t *)0xD0000100U)
#define FAULT_MAGIC      0xDEADBEEFUL

/*
 * Fault info layout at FAULT_DUMP_ADDR (0xD0000100):
 *   [0-7]   r0, r1, r2, r3, r12, lr, pc, xPSR (stacked exception frame)
 *   [8]     CFSR   [9] HFSR   [10] MMFAR   [11] BFAR
 *   [12]    AFSR   [13] SHCSR  [14] MAGIC=0xDEADBEEF
 *   [15]    EXC_RETURN (LR at handler entry; bit2=0→MSP, bit2=1→PSP)
 */

/*
 * Get the correct stack pointer for the exception frame.
 * FreeRTOS tasks use PSP, so we check EXC_RETURN bit 2.
 */
static uint32_t *fault_get_sp(void)
{
    uint32_t exc_return;
    __asm volatile ("MOV %0, LR" : "=r" (exc_return));
    if (exc_return & 0x4)
        return (uint32_t *)__get_PSP();
    else
        return (uint32_t *)__get_MSP();
}

static void fault_blink(uint32_t count)
{
    volatile uint32_t i, j;
    for (i = 0; i < count; i++)
    {
        HAL_GPIO_WritePin(BL_CTR_GPIO_Port, BL_CTR_Pin, GPIO_PIN_SET);
        for (j = 0; j < 5000000; j++) { __NOP(); }
        HAL_GPIO_WritePin(BL_CTR_GPIO_Port, BL_CTR_Pin, GPIO_PIN_RESET);
        for (j = 0; j < 5000000; j++) { __NOP(); }
    }
}

static void fault_pause(void)
{
    volatile uint32_t j;
    for (j = 0; j < 50000000; j++) { __NOP(); }
}

static void fault_long_pause(void)
{
    volatile uint32_t j;
    for (j = 0; j < 100000000; j++) { __NOP(); }
}

/*
 * Blink pattern: Preamble(3) → 2s → Category(N) → 2s → SubCode(M) → 4s → repeat
 *
 * Category: 1=HardFault  2=BusFault  3=MemManage  4=UsageFault
 * SubCode:  1=FORCED     2=PRECISERR 3=IMPRECISERR 4=DACCVIOL
 *           5=IACCVIOL   6=INVSTATE  7=INVPC  8=UNALIGNED  9=UNDEFINSTR 10=NOCP
 */
static void fault_show_backlight(void)
{
    if (LTDC) { LTDC->GCR &= ~LTDC_GCR_LTDCEN; }

    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    int category = 1, subcode = 1;

    if (hfsr & (1UL << 30)) /* FORCED */
    {
        subcode = 1;
        if (cfsr & 0x0000FF00UL)       category = 2;
        else if (cfsr & 0x000000FFUL)  category = 3;
        else if (cfsr & 0xFFFF0000UL)  category = 4;
    }
    else
    {
        if (cfsr & 0x0000FF00UL)
        {
            category = 2;
            if (cfsr & (1UL << 9))  subcode = 2;
            else if (cfsr & (1UL << 10)) subcode = 3;
        }
        else if (cfsr & 0x000000FFUL)
        {
            category = 3;
            if (cfsr & (1UL << 1))  subcode = 4;
            else if (cfsr & (1UL << 0)) subcode = 5;
        }
        else if (cfsr & 0xFFFF0000UL)
        {
            category = 4;
            if (cfsr & (1UL << 17))  subcode = 6;
            else if (cfsr & (1UL << 18)) subcode = 7;
            else if (cfsr & (1UL << 24)) subcode = 8;
            else if (cfsr & (1UL << 25)) subcode = 9;
            else if (cfsr & (1UL << 19)) subcode = 10;
            else subcode = 15;
        }
    }

    while (1)
    {
        fault_blink(3);    fault_pause();
        fault_blink(category); fault_pause();
        fault_blink(subcode); fault_long_pause();
    }
}

/* Helper: capture fault state to SDRAM */
static void fault_dump_to_sdram(uint32_t *sp)
{
    FAULT_DUMP_ADDR[0]  = sp[0];  /* r0 */
    FAULT_DUMP_ADDR[1]  = sp[1];  /* r1 */
    FAULT_DUMP_ADDR[2]  = sp[2];  /* r2 */
    FAULT_DUMP_ADDR[3]  = sp[3];  /* r3 */
    FAULT_DUMP_ADDR[4]  = sp[4];  /* r12 */
    FAULT_DUMP_ADDR[5]  = sp[5];  /* lr */
    FAULT_DUMP_ADDR[6]  = sp[6];  /* pc */
    FAULT_DUMP_ADDR[7]  = sp[7];  /* xPSR */
    FAULT_DUMP_ADDR[8]  = SCB->CFSR;
    FAULT_DUMP_ADDR[9]  = SCB->HFSR;
    FAULT_DUMP_ADDR[10] = SCB->MMFAR;
    FAULT_DUMP_ADDR[11] = SCB->BFAR;
    FAULT_DUMP_ADDR[12] = SCB->AFSR;
    FAULT_DUMP_ADDR[13] = SCB->SHCSR;
    FAULT_DUMP_ADDR[14] = FAULT_MAGIC;
    {
        uint32_t exc_return;
        __asm volatile ("MOV %0, LR" : "=r" (exc_return));
        FAULT_DUMP_ADDR[15] = exc_return;
    }
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA2D_HandleTypeDef hdma2d;
extern LTDC_HandleTypeDef hltdc;
extern SD_HandleTypeDef hsd2;
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  fault_dump_to_sdram(fault_get_sp());
  fault_show_backlight();
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  fault_dump_to_sdram(fault_get_sp());
  fault_show_backlight();
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  fault_dump_to_sdram(fault_get_sp());
  fault_show_backlight();
  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  fault_dump_to_sdram(fault_get_sp());
  fault_show_backlight();
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM6 global interrupt, DAC1_CH1 and DAC1_CH2 underrun error interrupts.
  */
void TIM6_DAC_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_DAC_IRQn 0 */

  /* USER CODE END TIM6_DAC_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_DAC_IRQn 1 */

  /* USER CODE END TIM6_DAC_IRQn 1 */
}

/**
  * @brief This function handles LTDC global interrupt.
  */
void LTDC_IRQHandler(void)
{
  /* USER CODE BEGIN LTDC_IRQn 0 */

  /* USER CODE END LTDC_IRQn 0 */
  HAL_LTDC_IRQHandler(&hltdc);
  /* USER CODE BEGIN LTDC_IRQn 1 */

  /* USER CODE END LTDC_IRQn 1 */
}

/**
  * @brief This function handles DMA2D global interrupt.
  */
void DMA2D_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2D_IRQn 0 */

  /* USER CODE END DMA2D_IRQn 0 */
  HAL_DMA2D_IRQHandler(&hdma2d);
  /* USER CODE BEGIN DMA2D_IRQn 1 */

  /* USER CODE END DMA2D_IRQn 1 */
}

/**
  * @brief This function handles SDMMC2 global interrupt.
  */
void SDMMC2_IRQHandler(void)
{
  /* USER CODE BEGIN SDMMC2_IRQn 0 */

  /* USER CODE END SDMMC2_IRQn 0 */
  HAL_SD_IRQHandler(&hsd2);
  /* USER CODE BEGIN SDMMC2_IRQn 1 */

  /* USER CODE END SDMMC2_IRQn 1 */
}

/**
  * @brief This function handles HSEM1 global interrupt.
  */
void HSEM1_IRQHandler(void)
{
  /* USER CODE BEGIN HSEM1_IRQn 0 */

  /* USER CODE END HSEM1_IRQn 0 */
  HAL_HSEM_IRQHandler();
  /* USER CODE BEGIN HSEM1_IRQn 1 */

  /* USER CODE END HSEM1_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
