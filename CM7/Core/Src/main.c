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
#include "crc.h"
#include "dma.h"
#include "dma2d.h"
#include "fatfs.h"
#include "i2c.h"
#include "ltdc.h"
#include "sdmmc.h"
#include "tim.h"
#include "gpio.h"
#include "fmc.h"
#include "app_touchgfx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "crc.h"
#include "dma2d.h"
#include "fmc.h"
#include "i2c.h"
#include "ltdc.h"
#include "sdmmc.h"
#include "tim.h"
#include "delay.h"
#include "lcd.h"
#include "string.h"
#include "bsp_driver_sd.h"
#include "NanoEdgeAI.h"
#include "shared_buf.h"
#include "shared_config.h"   /* proto_config_t / SHM_CONFIG / HSEM_ID_CONFIG（USER CODE PV 用 proto_config_t，须在 PV 前 include）*/
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DUAL_CORE_BOOT_SYNC_SEQUENCE  /* 双核 boot 同步：CM7 Release HSEM 唤醒 CM4 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0 */
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

//用来翻入输入数据的数组
float_t input_signal[NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER];
//模型输出每个分类类别的预测概率值
float_t probabilities[NEAI_NUMBER_OF_CLASSES];

uint8_t g_sd_nand_detected = 0;   /* 1 = SD NAND detected & ready */
HAL_SD_CardInfoTypeDef g_sd_card_info;
	uint32_t g_sd_free_kb = 0;
volatile uint32_t g_shm_rx_count = 0;   /* CM4→CM7 共享内存收到的字节数 */
volatile uint32_t g_sd_written = 0;     /* 已写入 SD NAND 的字节数（验证 SD 储存）*/
volatile uint32_t g_sd_status = 0;      /* SD init 状态：0未跑 1sdnand失败 2mount失败 3open失败 4就绪 */
volatile uint8_t g_record_req = 0;      /* 4 协议按钮请求：0=无, 1=UART, 2=SPI, 3=I2C, 4=CAN */
volatile uint8_t g_record_active = 0;   /* 当前记录的协议：0=没记, 1-4=协议 */
volatile uint8_t g_record_discard = 0;  /* 停止记录时是否删除文件：0=保留(f_close), 1=删除(f_unlink) — modal"不保存"设 */

/* SD 文件列表（SD_Test_Screen 显示 + selector + Delete）*/
file_entry_t g_file_list[MAX_FILES];
volatile uint8_t g_file_count = 0;
volatile uint8_t g_file_sel = 0;
volatile uint8_t g_file_refresh = 1;    /* 启动后请求扫一次 */
volatile uint8_t g_file_delete = 0;

/* 波形回放（SD_Test_Screen 点 Play → data_screen 读文件画波）*/
uint8_t g_playback_buf[PLAYBACK_BUF_SIZE];
volatile uint32_t g_playback_pos = 0;
volatile uint32_t g_playback_len = 0;
volatile uint8_t  g_playback_mode = 0;
volatile uint8_t  g_playback_req = 0;
volatile uint8_t  g_playback_stop = 0;
volatile uint8_t  g_playback_pause = 0;
volatile uint8_t  g_playback_step = 0;
volatile uint8_t  g_playback_file_idx = 0;
volatile uint32_t g_playback_file_size = 0;
volatile uint32_t g_playback_buf_start = 0;
volatile uint8_t  g_playback_reload = 0;
/* 录制配置 header（回放读出，data_screen 按录制时配置画波形 framing）*/
volatile proto_config_t g_playback_cfg;           /* 回放读出的录制时全部协议配置 */
volatile uint8_t  g_playback_cfg_valid = 0;       /* g_playback_cfg 是否有效(magic 匹配且读全) */
volatile uint32_t g_playback_header_len = 0;      /* 当前回放文件 header 长度(0=老文件无 header，seek 要偏移) */
volatile uint8_t  g_config_dirty = 0;     /* UI Apply 置 1 → defaultTask f_write config.bin 持久化（不直接写，避免和 sd_file 抢 FatFs）*/
volatile uint8_t  g_config_loaded = 0;    /* 上电 config-load 完成置 1（一次）。屏幕 tick 检测 0→1 跳变 → 重读 SHM_CONFIG 刷新 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* shared_config.h 已在 USER CODE Includes 区 include（PV 的 g_playback_cfg 用 proto_config_t）*/

/* 协议配置就绪通知:CM7 UI 写完 SHM_CONFIG 后调用,Release HSEM 通知 CM4 重配外设。
 * TouchGFX Settings_ScreenView 通过 extern "C" 调用(C++ 不直接碰 HAL)。*/
void shm_config_notify(void)
{
    __DSB();   /* 确保共享内存写入对 CM4 可见(non-cacheable 下双保险) */
    /* Take + Release 产生 free 事件 → 触发 CM4 HSEM 中断读 config。
     * 直接 Release 空闲态 semaphore 不触发中断，必须先 Take 锁定再释放。
     * CM4 不持有该 semaphore（FreeCallback 只置 flag 不 Take），Take 总成功。*/
    HAL_HSEM_Take(HSEM_ID_CONFIG, 0);
    HAL_HSEM_Release(HSEM_ID_CONFIG, 0);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  int32_t timeout;
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* Custom early init: cache enable + allow unaligned access (TouchGFX RGB565) */
  SCB->CCR &= ~SCB_CCR_UNALIGN_TRP_Msk;
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */
  delay_init(480);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_BDMA_Init();
  MX_FMC_Init();
  MX_SDMMC2_SD_Init();
  MX_DMA2D_Init();
  MX_I2C2_Init();
  MX_TIM3_Init();
  MX_CRC_Init();
  MX_LTDC_Init();
  MX_FATFS_Init();
  //MX_TouchGFX_Init();
  /* Call PreOsInit function */
  //MX_TouchGFX_PreOSInit();
  /* USER CODE BEGIN 2 */
  
  /* 恢复 regen 删除的共享内存 MPU region（双核 HSEM config/ring buffer + SDMMC2 DMA buffer 必须 non-cacheable）。
   * MPU_Config 生成区被 regen 覆盖只剩 Region 0/1，这里 USER CODE 区补回 Region 2/3/4。
   * SRAM4(D3)删了会导致 SDMMC2 DMA buffer cache 不一致 → HAL_SD_Init 失败 → SD 检测不到。*/
   HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_DONE));
  {
    MPU_Region_InitTypeDef MPU_Shared = {0};
    MPU_Shared.Enable = MPU_REGION_ENABLE;
    MPU_Shared.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_Shared.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_Shared.TypeExtField = MPU_TEX_LEVEL1;     /* TEX=1: shareable non-cacheable */
    MPU_Shared.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_Shared.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_Shared.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_Shared.SubRegionDisable = 0x0;
    MPU_Shared.Number = MPU_REGION_NUMBER2;        /* SRAM4 (D3 0x38000000): SDMMC2 DMA + 双核共享 */
    MPU_Shared.BaseAddress = 0x38000000;
    MPU_Shared.Size = MPU_REGION_SIZE_64KB;
    HAL_MPU_ConfigRegion(&MPU_Shared);
    MPU_Shared.Number = MPU_REGION_NUMBER3;        /* AXI SRAM (0x24060000): 双核共享段 */
    MPU_Shared.BaseAddress = 0x24060000;
    MPU_Shared.Size = MPU_REGION_SIZE_16KB;
    HAL_MPU_ConfigRegion(&MPU_Shared);
    MPU_Shared.Number = MPU_REGION_NUMBER4;        /* SRAM1 (D2 0x30000000): HSEM config + ring buffer */
    MPU_Shared.BaseAddress = 0x30000000;
    MPU_Shared.Size = MPU_REGION_SIZE_128KB;
    HAL_MPU_ConfigRegion(&MPU_Shared);
  }
  
  SHM_CONFIG->active_proto     = 4;
  SHM_CONFIG->can.baudrate     = 500000;
  SHM_CONFIG->can.mode         = 0;      // Normal
  SHM_CONFIG->can.tx_id        = 0x123;  // 发送到 ID=0x123
  SHM_CONFIG->can.tx_id_type   = 0;      // 标准帧
  SHM_CONFIG->can.tx_frame_type = 0;     // 数据帧
  SHM_CONFIG->can.tx_dlc       = 8;
  SHM_CONFIG->can.filter_mode  = 1;      // 精准过滤
  SHM_CONFIG->can.filter_id    = 0x321;  // 只收 ID=0x456
  SHM_CONFIG->can.filter_id_type = 0;    // 标准帧
  SHM_CONFIG->can.filter_fifo  = 0;      // FIFO0
  SCB_CleanDCache_by_Addr((uint32_t *)SHM_CONFIG_ADDR, sizeof(proto_config_t) + 64);
  SCB_CleanDCache_by_Addr((uint32_t *)SHM_TX_BUF_ADDR, 258);
  __DSB();
  HAL_HSEM_FastTake(HSEM_ID_CONFIG);

  HAL_HSEM_Release(HSEM_ID_CONFIG,0);
  
  /* SD NAND init is deferred to TouchGFX handleTickEvent().
   * HAL_SD_Init() blocks in SDMMC_GetCmdResp1() on this platform,
   * so we init manually in the task instead.                       */
  /* shm_init 改由 CM4 调用（CM4 main.c USER CODE 2）。
   * 原因：CM7→CM4 方向不通（shm_init 写停在 CM7 D-Cache，clean 也 flush 不到 CM4，
   * CM4 读 tail=3662 随机）。但 CM4→CM7 方向通（魔数 + 注释 shm_init 时 head=1029 验证）。
   * CM4 无 cache，shm_init 写直达物理，CM7 invalidate 能读到。*/

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

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
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
  MPU_InitStruct.BaseAddress = 0xD0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x38000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
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
