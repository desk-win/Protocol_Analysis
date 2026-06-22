/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @brief   SD Disk I/O driver — polling mode (FreeRTOS compatible)
  *
  *          Switched from DMA+RTOS template to polling to match the
  *          proven sdmmc_sdnand approach (实验29).  SDMMC2 interrupts
  *          are intentionally disabled; DMA is not used.
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

/* Note: originally generated as sd_diskio_dma_rtos_template_bspv1.c.
   Rewritten to polling for compatibility with SDMMC2 interrupt-disabled setup. */

/* USER CODE BEGIN firstSection */
/* can be used to modify / undefine following code or add new definitions */
/* USER CODE END firstSection*/

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "sd_diskio.h"

#include <string.h>
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define SD_DEFAULT_BLOCK_SIZE 512

/* USER CODE BEGIN enableSDDmaCacheMaintenance */
#define ENABLE_SD_DMA_CACHE_MAINTENANCE  1
/* USER CODE END enableSDDmaCacheMaintenance */

/* Private variables ---------------------------------------------------------*/
static volatile DSTATUS Stat = STA_NOINIT;

/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
#if _USE_WRITE == 1
DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
DRESULT SD_ioctl (BYTE, BYTE, void*);
#endif  /* _USE_IOCTL == 1 */

const Diskio_drvTypeDef  SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
#if  _USE_WRITE == 1
  SD_write,
#endif /* _USE_WRITE == 1 */

#if  _USE_IOCTL == 1
  SD_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* USER CODE BEGIN beforeFunctionSection */
/* can be used to modify / undefine following code or add new code */
/* USER CODE END beforeFunctionSection */

/* Private functions ---------------------------------------------------------*/

static DSTATUS SD_CheckStatus(BYTE lun)
{
  Stat = STA_NOINIT;

  if(BSP_SD_GetCardState() == SD_TRANSFER_OK)
  {
    Stat &= ~STA_NOINIT;
  }

  return Stat;
}

DSTATUS SD_initialize(BYTE lun)
{
  Stat = STA_NOINIT;

#if (osCMSIS <= 0x20000U)
  if(osKernelRunning())
#else
  if(osKernelGetState() == osKernelRunning)
#endif
  {
    if(BSP_SD_Init() == MSD_OK)
    {
      Stat = SD_CheckStatus(lun);
    }
  }

  return Stat;
}

DSTATUS SD_status(BYTE lun)
{
  return SD_CheckStatus(lun);
}

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;

#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
  uint32_t alignedAddr;
#endif

  /* SD NAND 偶发 CRC/timeout（f_getfree 每次返回不同值 = 读 FAT 得随机数据）。
   * 重试 3 次自愈——单次错误不再上抛 FR_DISK_ERR。*/
  for (int rd_retry = 0; rd_retry < 3; rd_retry++)
  {
    if (BSP_SD_ReadBlocks((uint32_t*)buff,
                          (uint32_t)(sector),
                          count,
                          SD_DATATIMEOUT) == MSD_OK)
    {
      res = RES_OK;

#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
      alignedAddr = (uint32_t)buff & ~0x1F;
      SCB_InvalidateDCache_by_Addr((uint32_t*)alignedAddr,
                                    count * SD_DEFAULT_BLOCK_SIZE + ((uint32_t)buff - alignedAddr));
#endif
      break;
    }
    /* 忙等延时（不依赖 SysTick，适用任意上下文）*/
    for (volatile int d = 0; d < 20000; d++);
  }

  return res;
}

#if _USE_WRITE == 1

DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;

#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
  uint32_t alignedAddr;
#endif

  /* SD NAND 偶发写入错误，重试 3 次（与 SD_read 对称）*/
  for (int wr_retry = 0; wr_retry < 3; wr_retry++)
  {
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
    alignedAddr = (uint32_t)buff & ~0x1F;
    SCB_CleanDCache_by_Addr((uint32_t*)alignedAddr,
                              count * SD_DEFAULT_BLOCK_SIZE + ((uint32_t)buff - alignedAddr));
#endif

    if (BSP_SD_WriteBlocks((uint32_t*)buff,
                           (uint32_t)(sector),
                           count,
                           SD_DATATIMEOUT) == MSD_OK)
    {
      res = RES_OK;
      break;
    }
    for (volatile int d = 0; d < 20000; d++);
  }

  return res;
}
#endif /* _USE_WRITE == 1 */

#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  BSP_SD_CardInfo CardInfo;

  if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (cmd)
  {
  case CTRL_SYNC :
    res = RES_OK;
    break;

  case GET_SECTOR_COUNT :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockNbr;
    res = RES_OK;
    break;

  case GET_SECTOR_SIZE :
    BSP_SD_GetCardInfo(&CardInfo);
    *(WORD*)buff = CardInfo.LogBlockSize;
    res = RES_OK;
    break;

  case GET_BLOCK_SIZE :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
    res = RES_OK;
    break;

  default:
    res = RES_PARERR;
  }

  return res;
}
#endif /* _USE_IOCTL == 1 */

/* USER CODE BEGIN lastSection */
/* can be used to modify / undefine following code or add new code */
/* USER CODE END lastSection */
