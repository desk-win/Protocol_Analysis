#include "sys_util.h"

uint8_t get_icahce_sta(void)
{
    uint8_t sta;
    sta = ((SCB->CCR)>>17) & 0X01;
    return sta;
}

uint8_t get_dcahce_sta(void)
{
    uint8_t sta;
    sta = ((SCB->CCR)>>16) & 0X01;
    return sta;
}

void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset)
{
    SCB->VTOR = baseaddr | (offset & (uint32_t)0xFFFFFE00);
}

void sys_wfi_set(void)
{
    __ASM volatile("wfi");
}

void sys_intx_disable(void)
{
    __ASM volatile("cpsid i");
}

void sys_intx_enable(void)
{
    __ASM volatile("cpsie i");
}

void sys_msr_msp(uint32_t addr)
{
    __set_MSP(addr);
}

void sys_cache_enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
    SCB->CACR |= 1 << 2;     /* Force D-Cache write-through */
}

uint8_t sys_stm32_clock_init(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq)
{
    HAL_StatusTypeDef ret = HAL_OK;
    RCC_ClkInitTypeDef rcc_clk_init_handle;
    RCC_OscInitTypeDef rcc_osc_init_handle;
    RCC_PeriphCLKInitTypeDef rcc_periph_clk_init;

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* HSE 25MHz as PLL source, HSI48 on */
    rcc_osc_init_handle.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    rcc_osc_init_handle.HSEState = RCC_HSE_ON;
    rcc_osc_init_handle.HSIState = RCC_HSI_OFF;
    rcc_osc_init_handle.CSIState = RCC_CSI_OFF;
    rcc_osc_init_handle.HSI48State = RCC_HSI48_ON;
    rcc_osc_init_handle.PLL.PLLState = RCC_PLL_ON;
    rcc_osc_init_handle.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    rcc_osc_init_handle.PLL.PLLN = plln;
    rcc_osc_init_handle.PLL.PLLM = pllm;
    rcc_osc_init_handle.PLL.PLLP = pllp;
    rcc_osc_init_handle.PLL.PLLQ = pllq;
    rcc_osc_init_handle.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    rcc_osc_init_handle.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    rcc_osc_init_handle.PLL.PLLFRACN = 0;
    ret = HAL_RCC_OscConfig(&rcc_osc_init_handle);

    if (ret != HAL_OK) return 1;

    /* System clock: PLL1 p = 480MHz, HCLK = 240MHz, APB = 120MHz */
    rcc_clk_init_handle.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_D3PCLK1);
    rcc_clk_init_handle.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    rcc_clk_init_handle.SYSCLKDivider = RCC_SYSCLK_DIV1;
    rcc_clk_init_handle.AHBCLKDivider = RCC_HCLK_DIV2;
    rcc_clk_init_handle.APB1CLKDivider = RCC_APB1_DIV2;
    rcc_clk_init_handle.APB2CLKDivider = RCC_APB2_DIV2;
    rcc_clk_init_handle.APB3CLKDivider = RCC_APB3_DIV2;
    rcc_clk_init_handle.APB4CLKDivider = RCC_APB4_DIV2;
    ret = HAL_RCC_ClockConfig(&rcc_clk_init_handle, FLASH_LATENCY_4);

    if (ret != HAL_OK) return 1;

    /* PLL2: 25/25*440=440MHz VCO, /2=220MHz PLL2R for FMC */
    rcc_periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_FMC;
    rcc_periph_clk_init.PLL2.PLL2M = 25;
    rcc_periph_clk_init.PLL2.PLL2N = 440;
    rcc_periph_clk_init.PLL2.PLL2P = 2;
    rcc_periph_clk_init.PLL2.PLL2R = 2;
    rcc_periph_clk_init.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_0;
    rcc_periph_clk_init.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    rcc_periph_clk_init.PLL2.PLL2FRACN = 0;
    rcc_periph_clk_init.FmcClockSelection = RCC_FMCCLKSOURCE_PLL2;
    ret = HAL_RCCEx_PeriphCLKConfig(&rcc_periph_clk_init);

    if (ret != HAL_OK) return 1;

    HAL_PWREx_EnableUSBVoltageDetector();
    __HAL_RCC_CSI_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_EnableCompensationCell();
    return 0;
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    while (1) {}
}
#endif
