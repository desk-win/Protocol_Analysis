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

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim6;
extern LTDC_HandleTypeDef hltdc;

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
  *
  * Captures complete fault state into a well-known SDRAM location for post-mortem
  * analysis.  The backlight blinks a pattern to identify the fault type even
  * without a debugger.
  *
  * Fault info layout at FAULT_DUMP_ADDR (0xD0000100):
  *   [0]  r0   (stacked)
  *   [1]  r1
  *   [2]  r2
  *   [3]  r3
  *   [4]  r12
  *   [5]  lr   (stacked from exception frame)
  *   [6]  pc   (stacked — precise fault address)
  *   [7]  xPSR (stacked)
  *   [8]  CFSR (Configurable Fault Status Register)
  *   [9]  HFSR (HardFault Status Register)
  *  [10]  MMFAR (MemManage Fault Address Register)
  *  [11]  BFAR (BusFault Address Register)
  *  [12]  AFSR (Auxiliary Fault Status Register)
  *  [13]  SCB->SHCSR (System Handler Control and State Register)
  *  [14]  MAGIC = 0xDEADBEEF (valid-dump marker)
  *  [15]  EXC_RETURN (LR at handler entry; bit2=0→MSP, bit2=1→PSP)
  */
#define FAULT_DUMP_ADDR  ((volatile uint32_t *)0xD0000100U)
#define FAULT_MAGIC      0xDEADBEEFUL

/*
 * Fault type encoding — VISIBLE LED BLINK PATTERN
 *
 * Uses the backlight LED to signal fault diagnostics.
 * ALL phases use the same blink speed (~200ms ON, ~200ms OFF).
 * Phases are separated by 2-second dark gaps so you can count easily.
 *
 * Pattern: Preamble(3) → [2s dark] → Category(N) → [2s dark] → SubCode(M) → [4s dark] → repeat
 *
 * CATEGORY (what failed):
 *   1 = HardFault (direct, not escalated)
 *   2 = BusFault    (bad memory access — read SDRAM @ 0xD0000100 for BFAR)
 *   3 = MemManage   (MPU violation — read SDRAM @ 0xD0000100 for MMFAR)
 *   4 = UsageFault  (undefined instr, unaligned, etc.)
 *
 * SUB-CODE (why):
 *   1 = FORCED from a lower fault (check CFSR in SDRAM dump for which one)
 *   2 = BusFault: PRECISERR (BFAR shows exact address)
 *   3 = BusFault: IMPRECISERR (bufferred write, hard to trace)
 *   4 = MemManage: DACCVIOL (data access to forbidden area)
 *   5 = MemManage: IACCVIOL (execution from XN area)
 *   6 = UsageFault: INVSTATE (BX to non-thumb code)
 *   7 = UsageFault: INVPC   (loading bad value into PC)
 *   8 = UsageFault: UNALIGNED
 *   9 = UsageFault: UNDEFINSTR
 *  10 = UsageFault: NOCP
 *  15 = unknown / other
 */
static void fault_blink(uint32_t count)
{
    volatile uint32_t i, j;
    for (i = 0; i < count; i++)
    {
        HAL_GPIO_WritePin(BL_CTR_GPIO_Port, BL_CTR_Pin, GPIO_PIN_SET);
        for (j = 0; j < 5000000; j++) { __NOP(); }   /* ~200ms ON */
        HAL_GPIO_WritePin(BL_CTR_GPIO_Port, BL_CTR_Pin, GPIO_PIN_RESET);
        for (j = 0; j < 5000000; j++) { __NOP(); }   /* ~200ms OFF */
    }
}

static void fault_pause(void)   /* ~2 seconds dark */
{
    volatile uint32_t j;
    for (j = 0; j < 50000000; j++) { __NOP(); }
}

static void fault_long_pause(void)  /* ~4 seconds dark */
{
    volatile uint32_t j;
    for (j = 0; j < 100000000; j++) { __NOP(); }
}

static void fault_show_backlight(void)
{
    /* Disable LTDC to save power and stop any garbled display */
    if (LTDC)
    {
        LTDC->GCR &= ~LTDC_GCR_LTDCEN;
    }

    /* Read fault status registers */
    uint32_t cfsr = SCB->CFSR;   /* composite: UFSR[31:16] BFSR[15:8] MMFSR[7:0] */
    uint32_t hfsr = SCB->HFSR;

    int category = 1;
    int subcode  = 1;

    /*
     * Decode: WHAT failed?
     */
    if (hfsr & (1UL << 30))  /* FORCED: a lower fault escalated to HardFault */
    {
        subcode = 1;  /* 1 = escalation */
        if (cfsr & 0x0000FF00UL)            category = 2;  /* BusFault */
        else if (cfsr & 0x000000FFUL)       category = 3;  /* MemManage */
        else if (cfsr & 0xFFFF0000UL)       category = 4;  /* UsageFault */
    }
    else
    {
        /* Direct HardFault (very rare on Cortex-M7 — usually something fatal) */
        if (cfsr & 0x0000FF00UL)            /* BusFault bits */
        {
            category = 2;
            if (cfsr & (1UL << 9))  subcode = 2;  /* PRECISERR */
            else if (cfsr & (1UL << 10)) subcode = 3;  /* IMPRECISERR */
        }
        else if (cfsr & 0x000000FFUL)       /* MMFSR bits */
        {
            category = 3;
            if (cfsr & (1UL << 1))  subcode = 4;  /* DACCVIOL */
            else if (cfsr & (1UL << 0)) subcode = 5;  /* IACCVIOL */
        }
        else if (cfsr & 0xFFFF0000UL)       /* UFSR bits */
        {
            category = 4;
            if (cfsr & (1UL << 17))  subcode = 6;  /* INVSTATE */
            else if (cfsr & (1UL << 18)) subcode = 7;  /* INVPC */
            else if (cfsr & (1UL << 24)) subcode = 8;  /* UNALIGNED */
            else if (cfsr & (1UL << 25)) subcode = 9;  /* UNDEFINSTR */
            else if (cfsr & (1UL << 19)) subcode = 10; /* NOCP */
            else subcode = 15;
        }
    }

    /* Safety clamp */
    if (category < 1) category = 1;
    if (category > 4) category = 4;
    if (subcode  < 1) subcode  = 1;
    if (subcode  > 15) subcode = 15;

    while (1)
    {
        /* ─── PREAMBLE: 3 blinks (always) ─── */
        fault_blink(3);
        fault_pause();   /* 2s dark — get ready to count */

        /* ─── CATEGORY: N blinks ─── */
        fault_blink(category);
        fault_pause();   /* 2s dark — get ready for next number */

        /* ─── SUB-CODE: M blinks ─── */
        fault_blink(subcode);
        fault_long_pause();  /* 4s dark — end of sequence */
    }
}

/*
 * Get the correct stack pointer for the exception frame.
 *
 * When a fault occurs in thread mode (FreeRTOS task), the CPU pushes the
 * exception frame (r0-r3, r12, lr, pc, xPSR) onto the thread's stack (PSP),
 * then switches to handler mode (MSP) to run the fault handler.
 *
 * EXC_RETURN (in LR) bit 2 tells us which stack holds the frame:
 *   bit2=0 → MSP,  bit2=1 → PSP
 */
static uint32_t *fault_get_sp(void)
{
    uint32_t exc_return;
    __asm volatile ("MOV %0, LR" : "=r" (exc_return));
    if (exc_return & 0x4)  /* bit 2 = 1: thread mode with PSP */
    {
        return (uint32_t *)__get_PSP();
    }
    else  /* handler mode or thread mode with MSP */
    {
        return (uint32_t *)__get_MSP();
    }
}

void HardFault_Handler(void)
{
    uint32_t *sp = fault_get_sp();

    uint32_t stacked_r0  = sp[0];
    uint32_t stacked_r1  = sp[1];
    uint32_t stacked_r2  = sp[2];
    uint32_t stacked_r3  = sp[3];
    uint32_t stacked_r12 = sp[4];
    uint32_t stacked_lr  = sp[5];
    uint32_t stacked_pc  = sp[6];
    uint32_t stacked_psr = sp[7];

    /* Capture fault status registers */
    uint32_t cfsr  = SCB->CFSR;
    uint32_t hfsr  = SCB->HFSR;
    uint32_t mmfar = SCB->MMFAR;
    uint32_t bfar  = SCB->BFAR;
    uint32_t afsr  = SCB->AFSR;
    uint32_t shcsr = SCB->SHCSR;

    /* Write to SDRAM */
    FAULT_DUMP_ADDR[0]  = stacked_r0;
    FAULT_DUMP_ADDR[1]  = stacked_r1;
    FAULT_DUMP_ADDR[2]  = stacked_r2;
    FAULT_DUMP_ADDR[3]  = stacked_r3;
    FAULT_DUMP_ADDR[4]  = stacked_r12;
    FAULT_DUMP_ADDR[5]  = stacked_lr;
    FAULT_DUMP_ADDR[6]  = stacked_pc;
    FAULT_DUMP_ADDR[7]  = stacked_psr;
    FAULT_DUMP_ADDR[8]  = cfsr;
    FAULT_DUMP_ADDR[9]  = hfsr;
    FAULT_DUMP_ADDR[10] = mmfar;
    FAULT_DUMP_ADDR[11] = bfar;
    FAULT_DUMP_ADDR[12] = afsr;
    FAULT_DUMP_ADDR[13] = shcsr;
    FAULT_DUMP_ADDR[14] = FAULT_MAGIC;
    {
        uint32_t exc_return;
        __asm volatile ("MOV %0, LR" : "=r" (exc_return));
        FAULT_DUMP_ADDR[15] = exc_return;  /* EXC_RETURN: bit2=0→MSP, bit2=1→PSP */
    }

    fault_show_backlight();
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
    uint32_t *sp = fault_get_sp();
    FAULT_DUMP_ADDR[0]  = sp[0];
    FAULT_DUMP_ADDR[1]  = sp[1];
    FAULT_DUMP_ADDR[2]  = sp[2];
    FAULT_DUMP_ADDR[3]  = sp[3];
    FAULT_DUMP_ADDR[4]  = sp[4];
    FAULT_DUMP_ADDR[5]  = sp[5];
    FAULT_DUMP_ADDR[6]  = sp[6];
    FAULT_DUMP_ADDR[7]  = sp[7];
    FAULT_DUMP_ADDR[8]  = SCB->CFSR;
    FAULT_DUMP_ADDR[9]  = SCB->HFSR;
    FAULT_DUMP_ADDR[10] = SCB->MMFAR;
    FAULT_DUMP_ADDR[11] = SCB->BFAR;
    FAULT_DUMP_ADDR[12] = SCB->AFSR;
    FAULT_DUMP_ADDR[13] = SCB->SHCSR;
    FAULT_DUMP_ADDR[14] = FAULT_MAGIC;
    fault_show_backlight();
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
    uint32_t *sp = fault_get_sp();
    FAULT_DUMP_ADDR[0]  = sp[0];
    FAULT_DUMP_ADDR[1]  = sp[1];
    FAULT_DUMP_ADDR[2]  = sp[2];
    FAULT_DUMP_ADDR[3]  = sp[3];
    FAULT_DUMP_ADDR[4]  = sp[4];
    FAULT_DUMP_ADDR[5]  = sp[5];
    FAULT_DUMP_ADDR[6]  = sp[6];
    FAULT_DUMP_ADDR[7]  = sp[7];
    FAULT_DUMP_ADDR[8]  = SCB->CFSR;
    FAULT_DUMP_ADDR[9]  = SCB->HFSR;
    FAULT_DUMP_ADDR[10] = SCB->MMFAR;
    FAULT_DUMP_ADDR[11] = SCB->BFAR;
    FAULT_DUMP_ADDR[12] = SCB->AFSR;
    FAULT_DUMP_ADDR[13] = SCB->SHCSR;
    FAULT_DUMP_ADDR[14] = FAULT_MAGIC;
    fault_show_backlight();
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
    uint32_t *sp = fault_get_sp();
    FAULT_DUMP_ADDR[0]  = sp[0];
    FAULT_DUMP_ADDR[1]  = sp[1];
    FAULT_DUMP_ADDR[2]  = sp[2];
    FAULT_DUMP_ADDR[3]  = sp[3];
    FAULT_DUMP_ADDR[4]  = sp[4];
    FAULT_DUMP_ADDR[5]  = sp[5];
    FAULT_DUMP_ADDR[6]  = sp[6];
    FAULT_DUMP_ADDR[7]  = sp[7];
    FAULT_DUMP_ADDR[8]  = SCB->CFSR;
    FAULT_DUMP_ADDR[9]  = SCB->HFSR;
    FAULT_DUMP_ADDR[10] = SCB->MMFAR;
    FAULT_DUMP_ADDR[11] = SCB->BFAR;
    FAULT_DUMP_ADDR[12] = SCB->AFSR;
    FAULT_DUMP_ADDR[13] = SCB->SHCSR;
    FAULT_DUMP_ADDR[14] = FAULT_MAGIC;
    fault_show_backlight();
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
  HAL_LTDC_IRQHandler(&hltdc);
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
