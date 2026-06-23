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
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_touchgfx.h"   /* provides TouchGFX_Task() */
#include "FreeRTOSConfig.h"
#include "shared_buf.h"
#include "fatfs.h"          /* f_mount/f_open/f_write + SDFatFS/SDPath/FIL */
#include "sdmmc.h"          /* hsd2（HAL_SD_GetCardInfo 填容量给 SD_Test_Screen）*/
#include <stdio.h>          /* snprintf（文件序号命名）*/
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
/* USER CODE BEGIN Variables */
/* TouchGFX task: static allocation with stack in AXI SRAM.
 * 48KB stack is placed in AXI SRAM via xTaskCreateStatic to avoid
 * consuming DTCM and to sidestep SDRAM bus contention issues. */
#define TOUCHGFX_STACK_WORDS  12288  /* 48 KB */
static StaticTask_t touchgfxTCB;
__attribute__((section(".axi_stack"))) static StackType_t touchgfxStack[TOUCHGFX_STACK_WORDS];
osThreadId_t touchgfxTaskHandle;

/* FreeRTOS heap in AXI SRAM — avoids SDRAM/LTDC bus conflict */
__attribute__((section(".axi_heap"))) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for StartTouchGFXTa */
osThreadId_t StartTouchGFXTaHandle;
const osThreadAttr_t StartTouchGFXTa_attributes = {
  .name = "StartTouchGFXTa",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTask02(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of StartTouchGFXTa */
  StartTouchGFXTaHandle = osThreadNew(StartTask02, NULL, &StartTouchGFXTa_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* Create TouchGFX task with STATIC allocation (stack in AXI SRAM) */
  touchgfxTaskHandle = xTaskCreateStatic(
      TouchGFX_Task,
      "TouchGFX",
      TOUCHGFX_STACK_WORDS,
      NULL,
      osPriorityAboveNormal,
      touchgfxStack,
      &touchgfxTCB);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  extern volatile uint32_t g_shm_rx_count;  /* 定义在 main.c */
  extern volatile uint32_t g_sd_written;    /* 定义在 main.c，已写入 SD 字节数 */
  extern uint8_t sdnand_init(void);         /* SD_Test_ScreenView.cpp，非 static */
  static uint16_t read_pos = 0;
  static uint8_t sd_buf[512];               /* SD 写缓冲（满 512 一块写，高效）*/
  uint16_t sd_pos = 0;
  static FIL sd_file;   /* static：移出栈，避免 FatFs f_mount/f_open/f_write 栈深覆盖 sd_file 导致 INVALID_OBJECT */
  uint8_t sd_ready = 0;
  uint32_t flush_cnt = 0;

  /* 等 2 秒让 SD NAND 上电就绪 */
  osDelay(2000);

  /* SD NAND init + mount + open（恢复到之前稳定基线：f_mount 重试 5 + 固定 protocol.log）。
   * 之前这版 sd 能稳定涨，加序号命名 + BSP_SD_Init 重写后反而不稳定，先回基线定位。*/
  extern volatile uint32_t g_sd_status;
  uint8_t si = sdnand_init();
  if (si != 0U)
  {
    g_sd_status = 1U;
  }
  else
  {
    /* 填 SD 卡容量信息（给 SD_Test_Screen 显示）。defaultTask 是唯一 SD 访问者，
     * 不再让 sd_nand_full_test 抢 SD（那是之前不稳定的根因）。*/
    extern HAL_SD_CardInfoTypeDef g_sd_card_info;
    HAL_SD_GetCardInfo(&hsd2, &g_sd_card_info);

    /* f_mount 重试 5 次（原 __weak BSP_SD_Init，重试时 HAL_SD_Init 重复——这版稳定）*/
    FRESULT fr = FR_NOT_READY;
    for (int retry = 0; retry < 5; retry++)
    {
      fr = f_mount(&SDFatFS, SDPath, 1U);
      if (fr == FR_OK) break;
      osDelay(1000);
    }
    if (fr != FR_OK) g_sd_status = 2U;
    else
    {
      /* 算可用空间。配合 ffconf.h _FS_NOFSINFO=3（f_getfree 强制扫描 FAT，不信任 FSINFO）。
       * SD NAND FSINFO 损坏会让 f_getfree 返回 0 假满（但 f_open 能分配簇证伪）→ 扫描得真实值。
       * 重试 3 次仍返回 0/失败 → 用 物理容量−已写 估算兜底。*/
      extern uint32_t g_sd_free_kb;
      FATFS *fs;
      DWORD fre_clust;
      FRESULT gf = FR_DISK_ERR;
      for (int gf_retry = 0; gf_retry < 3; gf_retry++)
      {
        gf = f_getfree("0:", &fre_clust, &fs);
        if (gf == FR_OK && fre_clust > 0) break;
        osDelay(200);
      }
      if (gf == FR_OK && fre_clust > 0)
        g_sd_free_kb = (uint32_t)(fre_clust * fs->csize / 2U);
      else
      {
        uint32_t total_kb   = g_sd_card_info.LogBlockNbr / 2U;
        uint32_t written_kb = g_sd_written / 1024U;
        g_sd_free_kb = (total_kb > written_kb) ? (total_kb - written_kb) : 0U;
      }
      g_sd_status = 4U;  /* mount 成功 = SD 可用（按钮记录依赖此状态）*/
    }
  }

  /* 4 协议按钮记录控制（方案A：按钮控制 CM7 记录，CM4 持续发）*/
  extern volatile uint8_t g_record_req;     /* 按钮请求：0=无, 1=UART, 2=SPI, 3=I2C, 4=CAN */
  extern volatile uint8_t g_record_active;  /* 当前记录：0=没记, 1-4=协议 */
  static const char *proto_files[4] = {
    "0:protocol_uart.log", "0:protocol_spi.log", "0:protocol_i2c.log", "0:protocol_can.log"
  };

  /* 共享内存已通(CM4→CM7 单向)。CM7 用本地 read_pos 消费 data，
   * 不写共享 tail（CM7→CM4 方向不通）。字节同时累积到 SD 缓冲。*/
  for(;;)
  {
    /* 处理按钮请求：按同按钮=停止记录，按别的=切换到新协议 */
    if (g_record_req != 0U)
    {
      uint8_t req = g_record_req;
      g_record_req = 0U;
      if (g_sd_status == 4U)   /* SD ready 才处理 */
      {
        if (req == g_record_active)
        {
          /* 停止当前记录。g_record_discard=1 → f_unlink 删除文件（modal"不保存"）*/
          extern volatile uint8_t g_record_discard;
          uint8_t disc = g_record_discard;
          g_record_discard = 0U;
          if (sd_ready) { f_close(&sd_file); sd_ready = 0U; }
          if (disc == 1U) f_unlink(proto_files[req - 1U]);
          g_record_active = 0U;
          g_sd_written = 0U;   /* 字节数归零：旧文件结束，下次记录从 0 开始计 */
        }
        else if (req >= 1U && req <= 4U)
        {
          /* 切换：停当前 + 开新协议文件（f_open 重试 3 次，SD NAND 偶发 DISK_ERR）。
           * 失败设 st=30+FR 让 UI 可见，否则 g_record_active 不变像"按钮没反应"。*/
          if (sd_ready) f_close(&sd_file);
          FRESULT fo = FR_DISK_ERR;
          for (int fo_retry = 0; fo_retry < 3; fo_retry++)
          {
            fo = f_open(&sd_file, proto_files[req - 1U], FA_CREATE_ALWAYS | FA_WRITE);
            if (fo == FR_OK) break;
            osDelay(50);
          }
          if (fo == FR_OK)
          {
            sd_ready = 1U;
            g_record_active = req;
            g_sd_written = 0U;   /* 切换协议 = 开新文件，字节数归零重新计 */
          }
          else
          {
            g_sd_status = 30U + (uint32_t)fo;  /* f_open 失败诊断：30+FR（31=DISK_ERR...）*/
          }
        }
      }
    }

    SCB_InvalidateDCache_by_Addr((uint32_t*)SHM_BUF_ADDR, SHM_BUF_SIZE + 64);
    uint16_t head = SHM_RING->head;
    uint32_t got = 0;
    while (read_pos != head) {
      uint8_t byte = SHM_RING->data[read_pos];
      read_pos = (uint16_t)((read_pos + 1U) % SHM_BUF_SIZE);
      got++;
      if (sd_ready && sd_pos < 512U) {
        sd_buf[sd_pos++] = byte;
      }
    }
    if (got > 0U) g_shm_rx_count += got;

    /* 缓冲满 512 字节 → 块写入；或每 10 秒（100×100ms）定时 flush 剩余 + sync 落盘 */
    flush_cnt++;
    if (sd_ready && (sd_pos >= 512U || flush_cnt >= 100U))
    {
      if (sd_pos > 0U) {
        /* f_write 重试 3 次（SD NAND 偶发 DISK_ERR，否则 st=21）*/
        UINT bw = 0;
        FRESULT fw = FR_DISK_ERR;
        for (int fw_retry = 0; fw_retry < 3; fw_retry++)
        {
          fw = f_write(&sd_file, sd_buf, sd_pos, &bw);
          if (fw == FR_OK) break;
          osDelay(10);
        }
        if (fw == FR_OK)
        {
          g_sd_written += bw;
          /* 之前偶发 f_write 失败置的 20+ 错误码，成功一次即恢复 4。
           * 否则一次 DISK_ERR 让 st 永久=21，看着像持续故障（实际 SD NAND 偶发特性）。*/
          if (g_sd_status >= 20U && g_sd_status < 30U) g_sd_status = 4U;
        }
        else g_sd_status = 20U + (uint32_t)fw;   /* 20+FRESULT（21=DISK_ERR,9=INVALID_OBJECT...）*/
        sd_pos = 0;
      }
      f_sync(&sd_file);
      flush_cnt = 0U;
    }

    osDelay(100);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the StartTouchGFXTa thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void *argument)
{
  /* USER CODE BEGIN StartTask02 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask02 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* StartDefaultTask is defined above in CubeMX-generated code. */
/* USER CODE END Application */

