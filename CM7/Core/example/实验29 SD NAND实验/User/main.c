/**
 ****************************************************************************************************
 * @file        main.c
 * @version     V1.0
 * @brief       SD NAND 实验
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
#include "./BSP/KEY/key.h"
#include "./MALLOC/malloc.h"
#include "./BSP/SDNAND/sdmmc_sdnand.h"


/**
 * @brief       通过串口打印SD NAND相关信息
 * @param       无
 * @retval      无
 */
void show_sdnand_info(void)
{
    uint64_t card_capacity;                            /* SD NAND容量 */
    HAL_SD_CardCIDTypeDef sdnand_cid;

    HAL_SD_GetCardCID(&g_sdnand_handle, &sdnand_cid);  /* 获取CID */
    get_sdnand_info(&g_sdnand_info_handle);            /* 获取SD NAND信息 */

    switch (g_sdnand_info_handle.CardType)
    {
        case CARD_SDSC:
        {
            if (g_sdnand_info_handle.CardVersion == CARD_V1_X)
            {
                printf("Card Type:SDSC V1\r\n");
            }
            else if (g_sdnand_info_handle.CardVersion == CARD_V2_X)
            {
                printf("Card Type:SDSC V2\r\n");
            }
        }
            break;
        
        case CARD_SDHC_SDXC:
            printf("Card Type:SDHC\r\n");
            break;
    }

    card_capacity = (uint64_t)(g_sdnand_info_handle.LogBlockNbr) * (uint64_t)(g_sdnand_info_handle.LogBlockSize);  /* 计算SD卡容量 */
    printf("Card ManufacturerID:%d\r\n", sdnand_cid.ManufacturerID);                                               /* 制造商ID */
    printf("Card RCA:%d\r\n", g_sdnand_info_handle.RelCardAdd);                                                    /* 卡相对地址 */
    printf("LogBlockNbr:%d \r\n", (uint32_t)(g_sdnand_info_handle.LogBlockNbr));                                   /* 显示逻辑块数量 */
    printf("LogBlockSize:%d \r\n", (uint32_t)(g_sdnand_info_handle.LogBlockSize));                                 /* 显示逻辑块大小 */
    printf("Card Capacity:%d MB\r\n", (uint32_t)(card_capacity >> 20));                                            /* 显示容量 */
    printf("Card BlockSize:%d\r\n\r\n", g_sdnand_info_handle.BlockSize);                                           /* 显示块大小 */
}

/**
 * @brief       测试SD NAND的读取
 * @note        从secaddr地址开始,读取seccnt个扇区的数据
 * @param       secaddr : 扇区地址
 * @param       seccnt  : 扇区数
 * @retval      无
 */
void sdnand_test_read(uint32_t secaddr, uint32_t seccnt)
{
    uint32_t i;
    uint8_t *buf;
    uint8_t sta = 0;
  
    buf = mymalloc(SRAMIN, seccnt * 512);           /* 申请内存,从内部SRAM申请内存 */
    sta = sdnand_read_disk(buf, secaddr, seccnt);   /* 读取secaddr扇区开始的内容 */

    if (sta == 0)
    {
        lcd_show_string(30, 190, 200, 16, 16, "USART1 Sending Data...", BLUE);
        printf("SECTOR %d DATA:\r\n", secaddr);

        for (i = 0; i < seccnt * 512; i++)
        {
            printf("%x ", buf[i]);                  /* 打印secaddr开始的扇区数据 */
        }
        
        printf("\r\nDATA ENDED\r\n");
        lcd_show_string(30, 190, 200, 16, 16, "USART1 Send Data Over!", BLUE);
    }
    else
    {
        printf("err:%d\r\n", sta);
        lcd_show_string(30, 190, 200, 16, 16, "SD NAND read Failure! ", BLUE);
    }

    myfree(SRAMIN, buf);                            /* 释放内存 */
} 

/**
 * @brief       测试SD NAND的写入
 * @note        从secaddr地址开始,写入seccnt个扇区的数据
 *              慎用!! 最好写全是0XFF的扇区,否则可能损坏SD NAND.
 *
 * @param       secaddr : 扇区地址
 * @param       seccnt  : 扇区数
 * @retval      无
 */
void sdnand_test_write(uint32_t secaddr, uint32_t seccnt)
{
    uint32_t i;
    uint8_t *buf;
    uint8_t sta = 0;
  
    buf = mymalloc(SRAMIN, seccnt * 512);           /* 从内部SRAM申请内存 */

    for (i = 0; i < seccnt * 512; i++)              /* 初始化写入的数据,是3的倍数. */
    {
        buf[i] = i * 3;
    }

    sta = sdnand_write_disk(buf, secaddr, seccnt);  /* 从secaddr扇区开始写入seccnt个扇区内容 */

    if (sta == 0)
    {
        printf("Write over!\r\n");
    }
    else 
    {
        printf("err:%d\r\n", sta);
    }

    myfree(SRAMIN, buf);                            /* 释放内存 */
}

int main(void)
{ 
    uint8_t key;
    uint8_t t = 0;
    uint64_t card_capacity;                 /* SD NAND容量 */
  
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
    key_init();                             /* 初始化按键 */
  
    my_mem_init(SRAMIN);                    /* 初始化内部内存池(AXI) */
    my_mem_init(SRAMEX);                    /* 初始化外部内存池(SDRAM) */
    my_mem_init(SRAM12);                    /* 初始化SRAM12内存池(SRAM1+SRAM2) */
    my_mem_init(SRAM4);                     /* 初始化SRAM4内存池(SRAM4) */
    my_mem_init(SRAMDTCM);                  /* 初始化DTCM内存池(DTCM) */
    my_mem_init(SRAMITCM);                  /* 初始化ITCM内存池(ITCM) */
    
    lcd_show_string(30, 50, 200, 16, 16, "STM32H747", RED);
    lcd_show_string(30, 70, 200, 16, 16, "SD NAND TEST", RED);
    lcd_show_string(30, 90, 200, 16, 16, "WKS SMART", RED);
    lcd_show_string(30, 110, 200, 16, 16, "KEY0:Read Sector 0", RED); 
    
    while (sdnand_init())                   /* 检测不到SD NAND */
    {
        lcd_show_string(30, 150, 200, 16, 16, "SD NAND Error!", RED);
        delay_ms(500);
        lcd_show_string(30, 150, 200, 16, 16, "Please Check! ", RED);
        delay_ms(500);
        LED0_TOGGLE();                      /* LED0(绿灯)闪烁 */
    }
        
    /* 打印SD NAND相关信息 */
    show_sdnand_info(); 
    
    /* 检测SD NAND成功 */
    lcd_show_string(30, 150, 200, 16, 16, "SD NAND OK    ", BLUE);
    lcd_show_string(30, 170, 200, 16, 16, "SD NAND Size:      MB", BLUE);
    card_capacity = (uint64_t)(g_sdnand_info_handle.LogBlockNbr) * (uint64_t)(g_sdnand_info_handle.LogBlockSize); /* 计算SD NAND容量 */
    lcd_show_num(30 + 13 * 8, 170, card_capacity >> 20, 6, 16, BLUE); /* 显示SD NAND容量(MB) */
    
    while (1)
    {        
        key = key_scan(0);

        if (key == KEY0_PRES)        /* KEY0按下了 */
        {
            sdnand_test_read(0, 1);  /* 从SD NAND的0扇区读取1*512字节的内容 */
        }

        t++;
        delay_ms(10);

        if (t == 20)
        {
            LED0_TOGGLE();           /* LED0(绿灯)闪烁 */
            t = 0;
        }
    }
}




