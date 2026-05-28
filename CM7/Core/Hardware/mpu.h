/**
 * MPU 内存保护 — 7 区域配置, 控制各地址段的 Cache/共享/执行属性
 * ============================================================
 * 一键配置:     mpu_memory_protection()       配置全部 7 个 MPU 区域
 * 单区域配置:   mpu_set_protection(baseaddr, size, region_num, no_exec, access_perm,
 *                                   shareable, cacheable, bufferable)
 *
 * 7 个区域的默认配置:
 *   DTCM 128KB      → 可缓存, 不可共享
 *   AXI SRAM 512KB  → 可缓存, 不可共享
 *   SRAM1-3 512KB   → 可缓存, 不可共享
 *   SRAM4 64KB      → 可缓存, 不可共享
 *   FMC bank 64MB   → 不可缓存, 不可共享
 *   SDRAM 32MB      → 可缓存, 不可共享 (framebuffer 在此, 必须可缓存)
 *   NAND 256MB      → 不可缓存, 不可共享, 禁止执行
 */
#ifndef __MPU_H
#define __MPU_H

#include "sys.h"

uint8_t mpu_set_protection(uint32_t baseaddr, uint32_t size, uint32_t rnum, uint8_t de, uint8_t ap, uint8_t sen, uint8_t cen, uint8_t ben);
void mpu_memory_protection(void);

#endif
