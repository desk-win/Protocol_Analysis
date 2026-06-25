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
#include "shared_config.h"   /* proto_config_t / SHM_CONFIG / REC_MAGIC（录制 header 读写）*/
#include "fatfs.h"          /* f_mount/f_open/f_write + SDFatFS/SDPath/FIL */
#include "sdmmc.h"          /* hsd2（HAL_SD_GetCardInfo 填容量给 SD_Test_Screen）*/
#include <stdio.h>          /* snprintf（文件序号命名）*/
#include <string.h>         /* strrchr/strncpy/strncmp/strcmp（文件列表扫描）*/
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
  .stack_size = 2048 * 4,
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
  extern volatile proto_config_t g_playback_cfg;      /* 回放读出的录制时配置（main.c 定义）*/
  extern volatile uint8_t  g_playback_cfg_valid;
  extern volatile uint32_t g_playback_header_len;
  /* 文件名前缀分类：d_=数字信号(方波), a_=模拟信号(未来折线)。
   * 每协议保留 PROTO_FILE_COUNT 个文件，轮转序号（d_uart_1.log, d_uart_2.log, ...）。
   * 文件列表 f_opendir 按前缀判断类型，回放按类型选 widget。*/
  #define PROTO_FILE_COUNT 3   /* 每协议保留文件数（留余地，后续可改大）*/
  static const char *proto_names[4] = { "uart", "spi", "i2c", "can" };
  static uint8_t proto_seq[4] = {0, 0, 0, 0};   /* 每协议当前序号（0..PROTO_FILE_COUNT-1 轮转覆盖）*/
  static char current_path[24] = "";            /* 当前记录文件路径（停止时 f_unlink 用）*/

  /* 共享内存已通(CM4→CM7 单向)。CM7 用本地 read_pos 消费 data，
   * 不写共享 tail（CM7→CM4 方向不通）。字节同时累积到 SD 缓冲。*/
  for(;;)
  {
    /* 文件列表服务（defaultTask 唯一 SD 访问者）：仅没记录时(sd_ready=0)扫描/删除，
     * 避免和 sd_file 的 FatFs 共享状态冲突。UI 在 SD_Test_Screen 设 g_file_refresh/g_file_delete。*/
    if (g_sd_status == 4U && !sd_ready)
    {
      if (g_file_refresh)
      {
        g_file_refresh = 0;
        g_file_count = 0;
        DIR dir;
        FILINFO fno;
        /* f_opendir 重试 3 次（SD NAND 偶发 DISK_ERR，和 f_getfree/f_open 同特性，
         * 失败则 g_file_count=0，UI 显示 scanning，下轮 g_file_refresh 再触发）*/
        FRESULT od = FR_DISK_ERR;
        for (int od_retry = 0; od_retry < 3; od_retry++)
        {
          od = f_opendir(&dir, "0:");
          if (od == FR_OK) break;
          osDelay(50);
        }
        if (od == FR_OK)
        {
          while (g_file_count < MAX_FILES)
          {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
            const char *dot = strrchr(fno.fname, '.');
            if (dot && (strcmp(dot, ".log") == 0 || strcmp(dot, ".LOG") == 0))
            {
              strncpy(g_file_list[g_file_count].name, fno.fname, 23);
              g_file_list[g_file_count].name[23] = 0;
              if      (strncmp(fno.fname, "d_", 2) == 0) g_file_list[g_file_count].type = 0;  /* digital */
              else if (strncmp(fno.fname, "a_", 2) == 0) g_file_list[g_file_count].type = 1;  /* analog */
              else                                        g_file_list[g_file_count].type = 2;  /* 其他 */
              g_file_list[g_file_count].size_kb = fno.fsize / 1024;
              g_file_count++;
            }
          }
          f_closedir(&dir);
        }
        if (g_file_sel >= g_file_count) g_file_sel = 0;  /* sel 越界回 0 */
      }
      if (g_file_delete && g_file_count > 0 && g_file_sel < g_file_count)
      {
        uint8_t sel = g_file_sel;
        g_file_delete = 0;
        char path[28];
        snprintf(path, sizeof(path), "0:%s", g_file_list[sel].name);
        f_unlink(path);
        g_file_refresh = 1;  /* 删除后重扫刷新列表 */
      }
    }

    /* 回放服务：UI 点 Play 设 g_playback_req → 开文件读 buf；g_playback_stop → 关文件。
     * 回放期间用户在 data_screen，sd_ready=0（没记录），play_file 独立 FIL 不冲突。*/
    static FIL  play_file;
    static uint8_t play_opened = 0;
    if (g_playback_req && !play_opened)
    {
      g_playback_req = 0;
      if (g_sd_status == 4U && g_file_count > 0 && g_playback_file_idx < g_file_count)
      {
        char path[28];
        snprintf(path, sizeof(path), "0:%s", g_file_list[g_playback_file_idx].name);
        if (f_open(&play_file, path, FA_READ) == FR_OK)
        {
          /* 读配置 header：magic+proto_config_t。匹配则按录制时配置回放，否则老文件从头当波形 */
          UINT br_h = 0;
          uint32_t magic = 0;
          f_read(&play_file, &magic, sizeof(magic), &br_h);
          if (br_h == sizeof(magic) && magic == REC_MAGIC) {
            UINT br_c = 0;
            f_read(&play_file, (void*)&g_playback_cfg, sizeof(proto_config_t), &br_c);
            g_playback_cfg_valid = (br_c == sizeof(proto_config_t)) ? 1U : 0U;
            g_playback_header_len = REC_HEADER_LEN;
          } else {
            f_lseek(&play_file, 0);            /* 老文件无 header：从头当波形 */
            g_playback_cfg_valid = 0U;
            g_playback_header_len = 0U;
          }
          UINT br = 0;
          f_read(&play_file, g_playback_buf, PLAYBACK_BUF_SIZE, &br);
          g_playback_len = br;            /* 首块有效字节（header 后）*/
          g_playback_buf_start = 0;       /* buf 对应波形 [0, br) */
          g_playback_file_size = f_size(&play_file) - g_playback_header_len;  /* 波形净大小（进度条准）*/
          g_playback_pos = 0;
          g_playback_pause = 1;   /* 进回放默认暂停，用户点 Run 才自动播放 */
          g_playback_mode = 1;
          play_opened = 1;
        }
      }
    }
    /* 分块重读：UI pos 超出当前 buf 范围时请求 reload，f_lseek + f_read 新块 */
    if (g_playback_reload && play_opened)
    {
      g_playback_reload = 0;
      UINT br = 0;
      f_lseek(&play_file, g_playback_header_len + g_playback_pos);   /* header_len 偏移（老文件=0）*/
      f_read(&play_file, g_playback_buf, PLAYBACK_BUF_SIZE, &br);
      g_playback_buf_start = g_playback_pos;
      g_playback_len = br;
    }
    if (g_playback_stop && play_opened)
    {
      g_playback_stop = 0;
      f_close(&play_file);
      play_opened = 0;
      g_playback_mode = 0;
    }

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
          if (disc == 1U) f_unlink(current_path);   /* modal"不保存"删当前记录文件 */
          g_record_active = 0U;
          g_sd_written = 0U;   /* 字节数归零：旧文件结束，下次记录从 0 开始计 */
        }
        else if (req >= 1U && req <= 4U)
        {
          /* 切换：停当前 + 开新协议文件（轮转序号 d_uart_1/2/3.log，每协议 PROTO_FILE_COUNT 个）*/
          if (sd_ready) {
            f_close(&sd_file);
            extern volatile uint8_t g_record_discard;
            if (g_record_discard) { g_record_discard = 0U; f_unlink(current_path); }  /* modal"不保存"切换：删当前 */
          }
          uint8_t seq = proto_seq[req - 1U];
          snprintf(current_path, sizeof(current_path), "0:d_%s_%u.log",
                   proto_names[req - 1U], seq + 1U);
          proto_seq[req - 1U] = (uint8_t)((seq + 1U) % PROTO_FILE_COUNT);
          FRESULT fo = FR_DISK_ERR;
          for (int fo_retry = 0; fo_retry < 3; fo_retry++)
          {
            fo = f_open(&sd_file, current_path, FA_CREATE_ALWAYS | FA_WRITE);
            if (fo == FR_OK) break;
            osDelay(50);
          }
          if (fo == FR_OK)
          {
            /* 写配置 header：magic + 当时全部协议配置(proto_config_t)。
             * 回放时读出按此还原波形 framing，不随当前 live 配置变。 */
            SCB_InvalidateDCache_by_Addr((uint32_t*)SHM_CONFIG_ADDR, sizeof(proto_config_t) + 32);
            UINT bw_h = 0;
            uint32_t magic = REC_MAGIC;
            f_write(&sd_file, &magic, sizeof(magic), &bw_h);
            f_write(&sd_file, (const void*)SHM_CONFIG, sizeof(proto_config_t), &bw_h);
            f_sync(&sd_file);
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

