#include "mpu_util.h"
#include <stdio.h>

uint8_t mpu_set_protection(uint32_t baseaddr, uint32_t size, uint32_t rnum, uint8_t de, uint8_t ap, uint8_t sen, uint8_t cen, uint8_t ben)
{
    MPU_Region_InitTypeDef mpu_region_init_handle;
    HAL_MPU_Disable();

    mpu_region_init_handle.Enable = MPU_REGION_ENABLE;
    mpu_region_init_handle.Number = rnum;
    mpu_region_init_handle.BaseAddress = baseaddr;
    mpu_region_init_handle.Size = size;
    mpu_region_init_handle.SubRegionDisable = 0X00;
    mpu_region_init_handle.TypeExtField = MPU_TEX_LEVEL0;
    mpu_region_init_handle.AccessPermission = (uint8_t)ap;
    mpu_region_init_handle.DisableExec = de;
    mpu_region_init_handle.IsShareable = sen;
    mpu_region_init_handle.IsCacheable = cen;
    mpu_region_init_handle.IsBufferable = ben;
    HAL_MPU_ConfigRegion(&mpu_region_init_handle);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    return 0;
}

void mpu_memory_protection(void)
{
    /* DTCM, 128KB */
    mpu_set_protection( 0x20000000,
                        MPU_REGION_SIZE_128KB,
                        MPU_REGION_NUMBER1, 0,
                        MPU_REGION_FULL_ACCESS,
                        MPU_ACCESS_NOT_SHAREABLE,
                        MPU_ACCESS_CACHEABLE,
                        MPU_ACCESS_BUFFERABLE);

    /* AXI SRAM, 512KB */
    mpu_set_protection( 0x24000000,
                        MPU_REGION_SIZE_512KB,
                        MPU_REGION_NUMBER2, 0,
                        MPU_REGION_FULL_ACCESS,
                        MPU_ACCESS_NOT_SHAREABLE,
                        MPU_ACCESS_CACHEABLE,
                        MPU_ACCESS_BUFFERABLE);

    /* SRAM1~SRAM3, 512KB */
    mpu_set_protection( 0x30000000,
                        MPU_REGION_SIZE_512KB,
                        MPU_REGION_NUMBER3, 0,
                        MPU_REGION_FULL_ACCESS,
                        MPU_ACCESS_NOT_SHAREABLE,
                        MPU_ACCESS_CACHEABLE,
                        MPU_ACCESS_BUFFERABLE);

    /* SRAM4, 64KB */
    mpu_set_protection( 0x38000000,
                        MPU_REGION_SIZE_64KB,
                        MPU_REGION_NUMBER4, 0,
                        MPU_REGION_FULL_ACCESS,
                        MPU_ACCESS_NOT_SHAREABLE,
                        MPU_ACCESS_CACHEABLE,
                        MPU_ACCESS_BUFFERABLE);

    /* FMC bank, 64MB (non-cacheable) */
    mpu_set_protection( 0x60000000,
                        MPU_REGION_SIZE_64MB,
                        MPU_REGION_NUMBER5, 0,
                        MPU_REGION_FULL_ACCESS,
                        MPU_ACCESS_NOT_SHAREABLE,
                        MPU_ACCESS_NOT_CACHEABLE,
                        MPU_ACCESS_NOT_BUFFERABLE);

    /* SDRAM, 32MB (cacheable) */
    mpu_set_protection( 0XD0000000,
                        MPU_REGION_SIZE_32MB,
                        MPU_REGION_NUMBER6, 0,
                        MPU_REGION_FULL_ACCESS,
                        MPU_ACCESS_NOT_SHAREABLE,
                        MPU_ACCESS_CACHEABLE,
                        MPU_ACCESS_BUFFERABLE);

    /* NAND Flash, 256MB (non-cacheable, no exec) */
    mpu_set_protection( 0x80000000,
                        MPU_REGION_SIZE_256MB,
                        MPU_REGION_NUMBER7, 1,
                        MPU_REGION_FULL_ACCESS,
                        MPU_ACCESS_NOT_SHAREABLE,
                        MPU_ACCESS_NOT_CACHEABLE,
                        MPU_ACCESS_NOT_BUFFERABLE);
}
