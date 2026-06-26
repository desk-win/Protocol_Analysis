/**
 ****************************************************************************************************
 * @file        main.c
 * @version     V1.0
 * @brief       FATFS 实验
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    STM32H747XIH6小系统板
 *
 ****************************************************************************************************
 */
 
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/MPU/mpu.h"
#include "./BSP/SDRAM/sdram.h"
#include "./BSP/LCD/lcd.h"
#include "./USMART/usmart.h"
#include "./MALLOC/malloc.h"
#include "./BSP/NORFLASH/norflash.h"
#include "./BSP/SDMMC/sdmmc_sdcard.h"
#include "./BSP/SDNAND/sdmmc_sdnand.h"
#include "./FATFS/exfuns/exfuns.h"


int main(void)
{ 
    uint32_t total, free;
    uint8_t t = 0;
    uint8_t res = 0;
  
    sys_cache_enable();                     /* 使能L1-Cache */
    HAL_Init();                             /* 初始化HAL库 */
    sys_stm32_clock_init(192, 5, 2, 4);     /* 设置时钟, 480Mhz */
    delay_init(480);                        /* 延时初始化 */
    usart_init(115200);                     /* 初始化USART */  
    usmart_init(240);	                      /* 初始化USMART */    
    led_init();                             /* 初始化LED */
    mpu_memory_protection();                /* 保护相关存储区域 */
    sdram_init();                           /* 初始化SDRAM */
    lcd_init();                             /* 初始化LCD */
    norflash_init();                        /* 初始化W25Q256 */
    sdnand_init();                          /* 初始化SD NAND */
  
    my_mem_init(SRAMIN);                    /* 初始化内部内存池(AXI) */
    my_mem_init(SRAMEX);                    /* 初始化外部内存池(SDRAM) */
    my_mem_init(SRAM12);                    /* 初始化SRAM12内存池(SRAM1+SRAM2) */
    my_mem_init(SRAM4);                     /* 初始化SRAM4内存池(SRAM4) */
    my_mem_init(SRAMDTCM);                  /* 初始化DTCM内存池(DTCM) */
    my_mem_init(SRAMITCM);                  /* 初始化ITCM内存池(ITCM) */
    
    lcd_show_string(30, 50, 200, 16, 16, "STM32H747", RED);
    lcd_show_string(30, 70, 200, 16, 16, "FATFS TEST", RED);
    lcd_show_string(30, 90, 200, 16, 16, "WKS SMART", RED);
    lcd_show_string(30, 110, 200, 16, 16, "Use USMART for test", RED);
    
    while (sd_init())                       /* 检测不到SD卡 */
    {
        lcd_show_string(30, 150, 200, 16, 16, "SD Card Error!", RED);
        delay_ms(500);
        lcd_show_string(30, 150, 200, 16, 16, "Please Check! ", RED);
        delay_ms(500);
        LED0_TOGGLE();                      /* LED0(绿灯)闪烁 */
    }
        
    exfuns_init();                          /* 为fatfs相关变量申请内存 */
    f_mount(fs[0], "0:", 1);                /* 挂载SD卡 */    
    res = f_mount(fs[1], "1:", 1);          /* 挂载NOR FLASH */
    
    if (res == 0X0D)                        /* FLASH磁盘,FAT文件系统错误,重新格式化FLASH */
    {
        lcd_show_string(30, 150, 200, 16, 16, "Flash Disk Formatting...", RED);      /* 格式化FLASH */
        res = f_mkfs("1:", 0, 0, FF_MAX_SS);                                         /* 格式化FLASH,1:,盘符;0,使用默认格式化参数 */

        if (res == 0)
        {
            f_setlabel((const TCHAR *)"1:WKS");                                      /* 设置Flash磁盘的名字为：WKS */
            lcd_show_string(30, 150, 200, 16, 16, "Flash Disk Format Finish", RED);  /* 格式化完成 */
        }
        else
        {
            lcd_show_string(30, 150, 200, 16, 16, "Flash Disk Format Error ", RED);  /* 格式化失败 */
        }

        delay_ms(1000);
    }
   
    res = f_mount(fs[2], "2:", 1);          /* 挂载SD NAND */
    
    if (res == 0X0D)                        /* SD NAND磁盘,FAT文件系统错误,重新格式化SD NAND */
    {
        lcd_show_string(30, 150, 210, 16, 16, "SD NAND Disk Formatting...", RED);    /* 格式化SD NAND */
        res = f_mkfs("2:", 0, 0, FF_MAX_SS);                                         /* 格式化SD NAND,2:,盘符;0,使用默认格式化参数 */

        if (res == 0)
        {
            f_setlabel((const TCHAR *)"2:SD NAND");                                  /* 设置SD NAND磁盘的名字为：SD NAND */
            lcd_show_string(30, 150, 210, 16, 16, "SD NAND Disk Format Finish", RED);/* 格式化完成 */
        }
        else
        {
            lcd_show_string(30, 150, 210, 16, 16, "SD NAND Disk Format Error ", RED);/* 格式化失败 */
        }

        delay_ms(1000);
    }
    
    lcd_fill(30, 150, 240, 150 + 16, WHITE);                                         /* 清除显示 */

    while (exfuns_get_free((uint8_t *)"0:", &total, &free))                          /* 得到SD卡的总容量和剩余容量 */
    {
        lcd_show_string(30, 150, 200, 16, 16, "SD Card Fatfs Error!", RED);
        delay_ms(200);
        lcd_fill(30, 150, 240, 150 + 16, WHITE);                                     /* 清除显示 */
        delay_ms(200);
        LED0_TOGGLE();                                                               /* LED0(绿灯)闪烁 */
    }

    lcd_show_string(30, 150, 200, 16, 16, "FATFS OK!", BLUE);     
    lcd_show_string(30, 170, 200, 16, 16, "SD Total Size:      MB", BLUE);
    lcd_show_string(30, 190, 200, 16, 16, "SD  Free Size:      MB", BLUE);
    lcd_show_num(30 + 8 * 14, 170, total >> 10, 6, 16, BLUE);                        /* 显示SD卡总容量 MB */
    lcd_show_num(30 + 8 * 14, 190, free >> 10, 6, 16, BLUE);                         /* 显示SD卡剩余容量 MB */
    
    while (1)
    {        
        t++;
        delay_ms(10);

        if (t == 20)
        {
            t = 0;             
            LED0_TOGGLE();                                                           /* LED0(绿灯)闪烁 */
        }
    }
}




