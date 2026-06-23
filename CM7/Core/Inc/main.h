/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CTP_INT_Pin GPIO_PIN_5
#define CTP_INT_GPIO_Port GPIOB
#define LCD_CS_Pin GPIO_PIN_5
#define LCD_CS_GPIO_Port GPIOD
#define LCD_SCL_Pin GPIO_PIN_4
#define LCD_SCL_GPIO_Port GPIOD
#define LCD_SDA_Pin GPIO_PIN_3
#define LCD_SDA_GPIO_Port GPIOD
#define LCD_PWREN_Pin GPIO_PIN_11
#define LCD_PWREN_GPIO_Port GPIOI
#define LCD_RST_Pin GPIO_PIN_5
#define LCD_RST_GPIO_Port GPIOH
#define CTP_SCL_Pin GPIO_PIN_10
#define CTP_SCL_GPIO_Port GPIOB
#define CTP_SDA_Pin GPIO_PIN_11
#define CTP_SDA_GPIO_Port GPIOB
#define CTP_RST_Pin GPIO_PIN_12
#define CTP_RST_GPIO_Port GPIOB
#define BL_CTR_Pin GPIO_PIN_0
#define BL_CTR_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
extern uint8_t g_sd_nand_detected;           /* 1 = SD NAND detected & ready */
extern HAL_SD_CardInfoTypeDef g_sd_card_info; /* Card info (valid when detected) */
extern uint32_t g_sd_free_kb;                /* SD NAND free space in KB */

/* SD 文件列表（defaultTask f_opendir 扫描填充，SD_Test_Screen 只读显示 + selector 操作）
 * 文件名前缀分类：d_=数字信号(方波), a_=模拟信号(未来折线), 其他=2 */
#define MAX_FILES 16   /* 文件列表容量（4 协议 × PROTO_FILE_COUNT 3 = 12，留余地给旧文件/模拟/扩展）*/
typedef struct {
    char name[24];      /* 文件名（如 "d_uart.log"），不含 "0:" 前缀 */
    uint8_t type;       /* 0=digital(d_), 1=analog(a_), 2=其他 */
    uint32_t size_kb;   /* 文件大小 KB */
} file_entry_t;
extern file_entry_t g_file_list[MAX_FILES];
extern volatile uint8_t g_file_count;     /* 实际文件数（≤MAX_FILES）*/
extern volatile uint8_t g_file_sel;       /* selector 当前选中索引（UI 写）*/
extern volatile uint8_t g_file_refresh;   /* UI 设 1 请求 defaultTask 重扫文件列表 */
extern volatile uint8_t g_file_delete;    /* UI 设 1 请求 defaultTask 删除 g_file_sel 文件 */

/* 波形回放（SD_Test_Screen 点 Play → data_screen 回放模式读文件画波）*/
#define PLAYBACK_BUF_SIZE 4096   /* 回放字节缓冲（文件超 4KB 则只回放前 4KB）*/
extern uint8_t g_playback_buf[PLAYBACK_BUF_SIZE];
extern volatile uint32_t g_playback_pos;     /* UI 当前画到第几字节 */
extern volatile uint32_t g_playback_len;     /* buf 有效字节数 */
extern volatile uint8_t  g_playback_mode;    /* 0=实时抓取, 1=回放中 */
extern volatile uint8_t  g_playback_req;     /* UI 设 1 请求回放 g_file_list[idx] */
extern volatile uint8_t  g_playback_stop;    /* UI 设 1 请求停止回放 */
extern volatile uint8_t  g_playback_pause;   /* 1=暂停自动推进（手动 step）*/
extern volatile uint8_t  g_playback_step;    /* UI 设 1 手动推进 1 字节 */
extern volatile uint8_t  g_playback_file_idx;/* 回放文件在 g_file_list 的索引 */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
