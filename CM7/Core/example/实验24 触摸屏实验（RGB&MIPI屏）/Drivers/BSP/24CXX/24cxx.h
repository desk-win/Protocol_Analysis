/**
 ****************************************************************************************************
 * @file        24cxx.h
 * @version     V1.0
 * @brief       24CXX 驱动代码
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    STM32H747XIH6小系统板
 *
 ****************************************************************************************************
 */

#ifndef __24CXX_H
#define __24CXX_H

#include "./SYSTEM/sys/sys.h"


#define BL24C01     127
#define BL24C02     255
#define BL24C04     511
#define BL24C08     1023
#define BL24C16     2047
#define BL24C32     4095
#define BL24C64     8191
#define BL24C128    16383
#define BL24C256    32767

/* 开发板使用的是24C16，所以定义EE_TYPE为BL24C16 */
#define EE_TYPE     BL24C16

/******************************************************************************************/

void bl24cxx_init(void);                                            /* 初始化IIC */
uint8_t bl24cxx_check(void);                                        /* 检查器件 */
uint8_t bl24cxx_read_one_byte(uint16_t addr);                       /* 指定地址读取一个字节 */
void bl24cxx_write_one_byte(uint16_t addr, uint8_t data);           /* 指定地址写入一个字节 */
void bl24cxx_write(uint16_t addr, uint8_t *pbuf, uint16_t datalen); /* 从指定地址开始写入指定长度的数据 */
void bl24cxx_read(uint16_t addr, uint8_t *pbuf, uint16_t datalen);  /* 从指定地址开始读出指定长度的数据 */

#endif







