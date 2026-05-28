#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "dma.h"
#include "dma2d.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
#include "sys.h"
#include "delay.h"
#include "mpu.h"
#include "sdram.h"
#include "lcd.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
void MX_FREERTOS_Init(void);

int main(void)
{

  /* ---- Copied from Waiken-Smart example init sequence ---- */

  sys_cache_enable();                     /* Enable L1-Cache (with forced write-through) */

  HAL_Init();                             /* Init HAL */
  sys_stm32_clock_init(192, 5, 2, 4);     /* HSE 25MHz → PLL1 480MHz, PLL2R 220MHz→FMC */
  delay_init(480);                        /* SysTick-based delay init */

  /* CM4 core not used — dual-core boot sync skipped */

  /* ---- Peripheral init ---- */
  MX_DMA_Init();
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_TIM3_Init();
  MX_DMA2D_Init();

  mpu_memory_protection();                /* MPU: SDRAM cacheable, FMC non-cacheable, etc. */
  sdram_init();                           /* SDRAM: CAS=2, SDCLK=110MHz */
  lcd_init();                             /* LCD: auto-detect panel, init LTDC */

  /* ---- FreeRTOS ---- */
  osKernelInitialize();
  MX_FREERTOS_Init();
  osKernelStart();

  while (1) {}
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
