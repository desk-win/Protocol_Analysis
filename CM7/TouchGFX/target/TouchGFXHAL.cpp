/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : TouchGFXHAL.cpp
  ******************************************************************************
  * This file was created by TouchGFX Generator 4.26.1. This file is only
  * generated once! Delete this file from your project and re-generate code
  * using STM32CubeMX or change this file manually to update it.
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

#include <TouchGFXHAL.hpp>

/* USER CODE BEGIN TouchGFXHAL.cpp */

#include "ltdc_draw.h"

using namespace touchgfx;

/* Rendering pipeline diagnostics — incremented at each stage */
volatile uint32_t g_beginframe_count = 0;
volatile uint32_t g_flush_count      = 0;
volatile uint32_t g_rendered_pixels  = 0;  /* total area flushed */
volatile uint32_t g_hal_init_enter   = 0;  /* set when HAL init starts */
volatile uint32_t g_hal_init_done    = 0;  /* set when HAL init completes */

/*
 * Per-step init markers in SDRAM (0xD0000310–0xD0000330).
 * These let us see exactly which step of initialize() crashed.
 */
#define INIT_MARKER_BASE  ((volatile uint32_t *)0xD0000310U)
enum {
    MK_HAL_INIT_START       = 0,  /* entered TouchGFXHAL::initialize() */
    MK_NVIC_DISABLED        = 1,  /* LTDC+DMA2D NVIC disabled */
    MK_BEFORE_PARENT_INIT   = 2,  /* about to call parent initialize() */
    MK_PARENT_INIT_DONE     = 3,  /* parent initialize() returned */
    MK_NVIC_REDISABLED      = 4,  /* NVIC re-disabled after parent */
    MK_FB_ADDR_SET          = 5,  /* setFrameBufferStartAddresses done */
    MK_LAYER_SWITCHED       = 6,  /* ltdc_layer_switch done */
    MK_LTDC_INT_ENABLED     = 7,  /* LTDC line interrupt configured */
    MK_HAL_INIT_COMPLETE    = 8,  /* initialize() complete */
};

void TouchGFXHAL::initialize()
{
    volatile uint32_t *mk = INIT_MARKER_BASE;
    mk[MK_HAL_INIT_START] = 0xBB000001;
    g_hal_init_enter = 1;

    /*
     * PREVENT INTERRUPTS DURING INITIALIZATION
     *
     * The parent HAL::initialize() calls enableInterrupts() which enables
     * LTDC + DMA2D NVIC.  If an LTDC interrupt fires before the VSYNC
     * semaphore is created, OSWrappers::signalVSync() crashes.
     *
     * Strategy: disable both NVIC lines first, let the framework init
     * (which may enable them internally), then disable again, do our
     * custom setup, and only at the VERY END enable LTDC interrupt.
     */
    NVIC_DisableIRQ(LTDC_IRQn);
    NVIC_DisableIRQ(DMA2D_IRQn);

    /* Clear any stale LTDC interrupt flags */
    if (LTDC)
    {
        LTDC->ICR = 0x0000001F;  /* clear all LTDC interrupt flags */
    }

    mk[MK_NVIC_DISABLED]     = 0xBB000002;
    mk[MK_BEFORE_PARENT_INIT] = 0xBB000003;

    TouchGFXGeneratedHAL::initialize();  /* <— CRASH suspected inside here */

    mk[MK_PARENT_INIT_DONE]  = 0xBB000004;

    /*
     * The parent init may have re-enabled interrupts via enableInterrupts().
     * Force them off again until WE are ready.
     */
    NVIC_DisableIRQ(LTDC_IRQn);
    NVIC_DisableIRQ(DMA2D_IRQn);

    mk[MK_NVIC_REDISABLED]   = 0xBB000005;

    /* Use Layer 0 for TouchGFX — Layer 1 (0xD0100000) proved unreadable
       by LTDC despite correct CFBAR and LEN=1.  Layer 0 (0xD0000000)
       works correctly with both DMA2D and LTDC. */
    setFrameBufferStartAddresses((void*)0xD0000000, (void*)0, (void*)0);

    mk[MK_FB_ADDR_SET]       = 0xBB000006;

    /* Ensure Layer 0 is enabled (should already be, but be safe) */
    ltdc_layer_switch(0, 1);
    /* Keep Layer 1 disabled — it doesnʼt work */
    ltdc_layer_switch(1, 0);

    mk[MK_LAYER_SWITCHED]    = 0xBB000007;

    /* ── NOW enable LTDC Line Interrupt (everything is ready) ──────── */
    {
        uint16_t active_line = (LTDC->BPCR & LTDC_BPCR_AVBP_Msk) - 1;
        LTDC->LIPCR = active_line;
        LTDC->IER |= LTDC_IER_LIE;
        /* Clear any pending LTDC interrupt before enabling NVIC */
        __DSB();
        LTDC->ICR = 0x0000001F;
        NVIC_ClearPendingIRQ(LTDC_IRQn);
        NVIC_EnableIRQ(LTDC_IRQn);
    }

    mk[MK_LTDC_INT_ENABLED]  = 0xBB000008;
    mk[MK_HAL_INIT_COMPLETE] = 0xBB000009;
    g_hal_init_done = 1;
}

/**
 * Gets the frame buffer address used by the TFT controller.
 *
 * @return The address of the frame buffer currently being displayed on the TFT.
 */
uint16_t* TouchGFXHAL::getTFTFrameBuffer() const
{
    /* Use Layer 0 instead of Layer 1 — Layer 1 @ 0xD0100000 proved
       unreadable by LTDC (white screen despite correct CFBAR & LEN=1).
       Layer 0 @ 0xD0000000 works correctly. */
    return (uint16_t*)LTDC_Layer1->CFBAR;
}

/**
 * Sets the frame buffer address used by the TFT controller.
 *
 * @param [in] address New frame buffer address.
 */
void TouchGFXHAL::setTFTFrameBuffer(uint16_t* address)
{
    /* Write to Layer 0 CFBAR and trigger immediate reload */
    LTDC_Layer1->CFBAR = (uint32_t)address;
    LTDC->SRCR = (uint32_t)LTDC_SRCR_IMR;
}

/**
 * This function is called whenever the framework has performed a partial draw.
 *
 * @param rect The area of the screen that has been drawn, expressed in absolute coordinates.
 *
 * @see flushFrameBuffer().
 */
void TouchGFXHAL::flushFrameBuffer(const touchgfx::Rect& rect)
{
    g_flush_count++;
    g_rendered_pixels += (uint32_t)rect.width * (uint32_t)rect.height;
    TouchGFXGeneratedHAL::flushFrameBuffer(rect);
}

bool TouchGFXHAL::blockCopy(void* RESTRICT dest, const void* RESTRICT src, uint32_t numBytes)
{
    /* Use DMA2D M2M for hardware-accelerated memory copy.
       Falls back to parent's memcpy for very large or small transfers. */

    /* Ensure DMA2D clock is on */
    __HAL_RCC_DMA2D_CLK_ENABLE();

    /* Only use DMA2D for transfers ≥ 4 bytes (aligned 32-bit words).
       Small copies have too much setup overhead. */
    if (numBytes >= 4)
    {
        uint32_t numWords  = numBytes / 4;
        uint32_t remainder = numBytes % 4;

        dma2d_lock();

        /* Wait for any previous DMA2D op to complete */
        while ((READ_REG(DMA2D->CR) & DMA2D_CR_START) != 0U);
        WRITE_REG(DMA2D->IFCR, DMA2D_FLAG_TC | DMA2D_FLAG_CE | DMA2D_FLAG_TE);

        /* DMA2D M2M copy: 32-bit words, no pixel-format conversion */
        WRITE_REG(DMA2D->FGPFCCR, DMA2D_INPUT_ARGB8888);
        WRITE_REG(DMA2D->OPFCCR, DMA2D_OUTPUT_ARGB8888);
        WRITE_REG(DMA2D->FGMAR,  reinterpret_cast<uint32_t>(src));
        WRITE_REG(DMA2D->OMAR,  reinterpret_cast<uint32_t>(dest));
        WRITE_REG(DMA2D->FGOR,  0);
        WRITE_REG(DMA2D->OOR,   0);
        WRITE_REG(DMA2D->NLR,   numWords);   /* PL = numWords, NL = 1 */
        WRITE_REG(DMA2D->CR,    DMA2D_M2M | DMA2D_CR_START);

        /* Poll for completion */
        while ((READ_REG(DMA2D->ISR) & DMA2D_FLAG_TC) == 0U);
        WRITE_REG(DMA2D->IFCR, DMA2D_FLAG_TC);

        dma2d_unlock();

        /* Handle remaining 1-3 bytes with CPU */
        if (remainder > 0)
        {
            uint8_t*       d = (uint8_t*)dest + numWords * 4;
            const uint8_t* s = (const uint8_t*)src + numWords * 4;
            for (uint32_t i = 0; i < remainder; i++)
            {
                d[i] = s[i];
            }
        }

        return true;
    }

    /* Fall back to parent implementation for tiny transfers */
    return TouchGFXGeneratedHAL::blockCopy(dest, src, numBytes);
}

/**
 * Configures the interrupts relevant for TouchGFX. This primarily entails setting
 * the interrupt priorities for the DMA and LCD interrupts.
 */
void TouchGFXHAL::configureInterrupts()
{
    volatile uint32_t *mk = INIT_MARKER_BASE;
    mk[10] = 0xBB000010;  /* configureInterrupts called */

    TouchGFXGeneratedHAL::configureInterrupts();

    mk[11] = 0xBB000011;  /* configureInterrupts done */
}

void TouchGFXHAL::enableInterrupts()
{
    volatile uint32_t *mk = INIT_MARKER_BASE;
    mk[12] = 0xBB000012;  /* enableInterrupts called (enables NVIC!) */

    TouchGFXGeneratedHAL::enableInterrupts();

    mk[13] = 0xBB000013;  /* enableInterrupts done */
}

void TouchGFXHAL::disableInterrupts()
{
    TouchGFXGeneratedHAL::disableInterrupts();
}

void TouchGFXHAL::enableLCDControllerInterrupt()
{
    volatile uint32_t *mk = INIT_MARKER_BASE;
    mk[14] = 0xBB000014;  /* enableLCDControllerInterrupt called (sets LIE!) */

    TouchGFXGeneratedHAL::enableLCDControllerInterrupt();

    mk[15] = 0xBB000015;  /* enableLCDControllerInterrupt done */
}

bool TouchGFXHAL::beginFrame()
{
    g_beginframe_count++;
    return TouchGFXGeneratedHAL::beginFrame();
}

void TouchGFXHAL::endFrame()
{
    TouchGFXGeneratedHAL::endFrame();
}

/* USER CODE END TouchGFXHAL.cpp */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
