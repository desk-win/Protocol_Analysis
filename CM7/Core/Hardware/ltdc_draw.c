#include "ltdc_draw.h"

uint32_t *g_ltdc_framebuf[2];
_ltdc_dev lcdltdc;

/* ── DMA2D mutual exclusion (atomic, reentrant) ─────────────────────────────
 * Shared between bare-metal (polling) and TouchGFX (interrupt-driven queue +
 * paint functions).  Uses ARMv7-M LDREX/STREX to guarantee exactly one owner.
 *
 * REENTRANCY: TouchGFX paint functions (rgb565/rgb888 lineFrom*) call
 * dma2d_lock() once per line.  A 240-line draw would deadlock with a plain
 * spinlock.  We track nesting depth so that calls from the same owner are
 * no-ops after the first one.  dma2d_unlock_all() (called by tearDown())
 * resets the nesting counter and releases the underlying lock.
 *
 * IMPORTANT: Do NOT call dma2d_lock() from an ISR while the bare-metal task
 * holds the lock — the ISR will spin forever.  TouchGFX DMA2D ISR only
 * calls dma2d_unlock(), never lock.  bare-metal DMA2D calls are all polling
 * (no ISR), so this design is deadlock-free.
 */
static volatile uint32_t dma2d_locked = 0;
static uint32_t dma2d_nesting = 0;  /* not volatile — only accessed while locked */

void dma2d_lock(void)
{
    if (dma2d_nesting > 0) {
        /* Already own the lock — just bump the nesting counter */
        dma2d_nesting++;
        return;
    }

    uint32_t status;
    do {
        while (__LDREXW(&dma2d_locked) != 0) {}
        status = __STREXW(1, &dma2d_locked);
    } while (status != 0);
    __DMB();
    dma2d_nesting = 1;
}

void dma2d_unlock(void)
{
    if (dma2d_nesting > 1) {
        dma2d_nesting--;
        return;
    }
    dma2d_nesting = 0;
    __DMB();
    dma2d_locked = 0;
}

/* Full unlock — called by tearDown() to release ALL nested acquisitions */
void dma2d_unlock_all(void)
{
    dma2d_nesting = 0;
    __DMB();
    dma2d_locked = 0;
}

void ltdc_switch(uint8_t sw)
{
    if (sw)
        __HAL_LTDC_ENABLE(&hltdc);
    else
        __HAL_LTDC_DISABLE(&hltdc);
}

void ltdc_layer_switch(uint8_t layerx, uint8_t sw)
{
    if (sw)
        __HAL_LTDC_LAYER_ENABLE(&hltdc, layerx);
    else
        __HAL_LTDC_LAYER_DISABLE(&hltdc, layerx);

    __HAL_LTDC_RELOAD_CONFIG(&hltdc);
}

void ltdc_select_layer(uint8_t layerx)
{
    lcdltdc.activelayer = layerx;
}

void ltdc_display_dir(uint8_t dir)
{
    lcdltdc.dir = dir;

    if (dir == 0)
    {
        lcdltdc.width = lcdltdc.pheight;
        lcdltdc.height = lcdltdc.pwidth;
    }
    else if (dir == 1)
    {
        lcdltdc.width = lcdltdc.pwidth;
        lcdltdc.height = lcdltdc.pheight;
    }
}

void ltdc_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
    if (lcdltdc.dir)
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    else
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    if (lcdltdc.dir)
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
        *(uint8_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x) + 2) = color >> 16;
    }
    else
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
        *(uint8_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y) + 2) = color >> 16;
    }
#else
    if (lcdltdc.dir)
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    else
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
#endif
}

uint32_t ltdc_read_point(uint16_t x, uint16_t y)
{
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
    if (lcdltdc.dir)
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    else
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    if (lcdltdc.dir)
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) & 0XFFFFFF;
    else
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) & 0XFFFFFF;
#else
    if (lcdltdc.dir)
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    else
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
#endif
}

void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint32_t psx, psy, pex, pey;
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    if (lcdltdc.dir)
    {
        psx = sx; psy = sy; pex = ex; pey = ey;
    }
    else
    {
        if (ex >= lcdltdc.pheight) ex = lcdltdc.pheight - 1;
        if (sx >= lcdltdc.pheight) sx = lcdltdc.pheight - 1;
        psx = sy; psy = lcdltdc.pheight - ex - 1;
        pex = ey; pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));

    __HAL_RCC_DMA2D_CLK_ENABLE();

    dma2d_lock();
    DMA2D->CR = DMA2D_R2M;
    DMA2D->OPFCCR = LTDC_PIXFORMAT;
    DMA2D->OOR = offline;
    DMA2D->OMAR = addr;
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16);
    DMA2D->OCOLR = color;
    DMA2D->CR |= DMA2D_CR_START;

    while ((DMA2D->ISR & (DMA2D_FLAG_TC)) == 0)
    {
        timeout++;
        if (timeout > 0X1FFFFF) break;
    }

    DMA2D->IFCR |= DMA2D_FLAG_TC;
    dma2d_unlock();
}

void ltdc_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint32_t psx, psy, pex, pey;
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    if (lcdltdc.dir)
    {
        psx = sx; psy = sy; pex = ex; pey = ey;
    }
    else
    {
        psx = sy; psy = lcdltdc.pheight - ex - 1;
        pex = ey; pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));

    __HAL_RCC_DMA2D_CLK_ENABLE();

    dma2d_lock();
    DMA2D->CR = DMA2D_M2M;
    DMA2D->FGPFCCR = LTDC_PIXFORMAT;
    DMA2D->FGOR = 0;
    DMA2D->OOR = offline;
    DMA2D->FGMAR = (uint32_t)color;
    DMA2D->OMAR = addr;
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16);
    DMA2D->CR |= DMA2D_CR_START;

    while((DMA2D->ISR & (DMA2D_FLAG_TC)) == 0)
    {
        timeout++;
        if (timeout > 0X1FFFFF) break;
    }

    DMA2D->IFCR |= DMA2D_FLAG_TC;
    dma2d_unlock();
}

void ltdc_clear(uint32_t color)
{
    ltdc_fill(0, 0, lcdltdc.width - 1, lcdltdc.height - 1, color);
}
