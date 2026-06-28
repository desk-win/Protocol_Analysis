/**
  ******************************************************************************
  * File Name          : config_persist.c
  * Description        : 配置持久化：SD卡 config.bin 读写
  *                      独立于 CubeMX 生成文件，不会被覆盖
  ******************************************************************************
  */

#include "ff.h"             /* FIL, f_open, f_write, f_close, FA_WRITE, FA_CREATE_ALWAYS */
#include "shared_config.h"  /* SHM_CONFIG, proto_config_t, CFG_MAGIC */

/* 将当前 SHM_CONFIG 写入 SD 卡 config.bin（magic + proto_config_t 二进制）*/
void write_config_bin(void)
{
    FIL fil;
    UINT bw;
    uint32_t magic = CFG_MAGIC;
    if (f_open(&fil, "0:config.bin", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        f_write(&fil, &magic, sizeof(magic), &bw);
        f_write(&fil, (const void*)SHM_CONFIG, sizeof(proto_config_t), &bw);
        f_close(&fil);
    }
}
