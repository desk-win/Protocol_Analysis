/**
 * SDRAM 驱动 — W9825G6KH 32MB SDRAM 初始化与读写
 * ===============================================
 * 初始化:      sdram_init()                   CAS=2, SDCLK=110MHz (FMC=220MHz/2)
 * 写缓冲:      fmc_sdram_write_buffer(buf, addr, len)
 * 读缓冲:      fmc_sdram_read_buffer(buf, addr, len)
 * SDRAM 基址:  BANK6_SDRAM_ADDR = 0xD0000000
 * 全局句柄:    g_sdram_handle (SDRAM_HandleTypeDef)
 *
 * 注意: 必须在 MPU 配置为 SDRAM cacheable 之后再调用 sdram_init()
 */
#ifndef _SDRAM_H
#define _SDRAM_H

#include "sys.h"

extern SDRAM_HandleTypeDef g_sdram_handle;

#define BANK6_SDRAM_ADDR    ((uint32_t)(0XD0000000))

#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

void sdram_init(void);
void sdram_initialization_sequence(void);
uint8_t sdram_send_cmd(uint8_t bankx, uint8_t cmd, uint8_t refresh, uint16_t regval);
void fmc_sdram_write_buffer(uint8_t *pbuf, uint32_t writeaddr, uint32_t n);
void fmc_sdram_read_buffer(uint8_t *pbuf, uint32_t readaddr, uint32_t n);

#endif
