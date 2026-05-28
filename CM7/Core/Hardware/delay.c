#include "sys.h"
#include "delay.h"

static uint32_t g_fac_us = 0;

#if SYS_SUPPORT_OS
#include "os.h"
static uint16_t g_fac_ms = 0;

#define delay_osrunning     OSRunning
#define delay_ostickspersec OS_TICKS_PER_SEC
#define delay_osintnesting  OSIntNesting

void delay_osschedlock(void)  { OSSchedLock(); }
void delay_osschedunlock(void){ OSSchedUnlock(); }
void delay_ostimedly(uint32_t ticks) { OSTimeDly(ticks); }

void SysTick_Handler(void)
{
    if (delay_osrunning == OS_TRUE)
    {
        OS_CPU_SysTickHandler();
    }
    HAL_IncTick();
}
#endif

void delay_init(uint16_t sysclk)
{
#if SYS_SUPPORT_OS
    uint32_t reload;
#endif
    g_fac_us = sysclk;

    /* Use SysTick as free-running counter for delay_us (no interrupt) */
    SysTick->CTRL &= ~(1 << 0);
    SysTick->LOAD = 0x00FFFFFF;
    SysTick->VAL = 0;
    SysTick->CTRL = (1 << 2) | (1 << 0);  /* CLKSOURCE=core, ENABLE */

#if SYS_SUPPORT_OS
    reload = sysclk;
    reload *= 1000000 / delay_ostickspersec;
    g_fac_ms = 1000 / delay_ostickspersec;
    SysTick->CTRL |= 1 << 1;
    SysTick->LOAD = reload;
    SysTick->CTRL |= 1 << 0;
#endif
}

void delay_us(uint32_t nus)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;
    ticks = nus * g_fac_us;

#if SYS_SUPPORT_OS
    delay_osschedlock();
#endif

    told = SysTick->VAL;

    while (1)
    {
        tnow = SysTick->VAL;

        if (tnow != told)
        {
            if (tnow < told)
                tcnt += told - tnow;
            else
                tcnt += reload - tnow + told;

            told = tnow;

            if (tcnt >= ticks)
                break;
        }
    }

#if SYS_SUPPORT_OS
    delay_osschedunlock();
#endif
}

void delay_ms(uint16_t nms)
{
#if SYS_SUPPORT_OS
    if (delay_osrunning && delay_osintnesting == 0)
    {
        if (nms >= g_fac_ms)
            delay_ostimedly(nms / g_fac_ms);
        nms %= g_fac_ms;
    }
#endif
    delay_us((uint32_t)(nms * 1000));
}

void HAL_Delay(uint32_t Delay)
{
    delay_ms(Delay);
}
