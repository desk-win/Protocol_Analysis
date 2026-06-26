/**
 ****************************************************************************************************
 * @file        mipi_lcd.h
 * @version     V1.2
 * @brief       mipilcd 驱动代码
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    STM32H747XIH6小系统板
 *
 * V1.1
 * 修改了一些MIPI屏的参数配置
 *
 * V1.2
 * 修改了一些MIPI屏的参数配置
 * 添加对5.5寸1080P MIPI屏的兼容
 *
 ****************************************************************************************************
 */

#ifndef __MIPI_LCD_H
#define __MIPI_LCD_H

#include "./SYSTEM/sys/sys.h"


/* 选择屏幕尺寸 */
#define MIPILCD_35_320480 	 0		/* 3.5 寸MIPI屏       驱动IC: ST7796   */
#define MIPILCD_50_7201280 	 1		/* 5   寸720P MIPI屏  驱动IC: ILI9881D */
#define MIPILCD_55_7201280 	 0		/* 5.5 寸720P MIPI屏  驱动IC: ST7703   */
#define MIPILCD_55_10801920  0		/* 5.5 寸1080P MIPI屏 驱动IC: HX8399   */
#define MIPILCD_70_1024600 	 0		/* 7   寸MIPI屏       驱动IC: EK79007  */
#define MIPILCD_80_8001280 	 0  	/* 8   寸800P MIPI屏  驱动IC: ILI9881C */
#define MIPILCD_10_8001280 	 0		/* 10.1寸800P MIPI屏  驱动IC: ILI9881C */

/* 3.5寸320*480分辨率的参数配置 */
#if MIPILCD_35_320480      
#define LCD_DEFAULT_WIDTH       320
#define LCD_DEFAULT_HEIGHT      480

#define  MIPI_HSYNC             20      /* Horizontal synchronization */
#define  MIPI_HBP               60      /* Horizontal back porch      */
#define  MIPI_HFP               60      /* Horizontal front porch     */
#define  MIPI_VSYNC             60      /* Vertical synchronization   */
#define  MIPI_VBP               80      /* Vertical back porch        */
#define  MIPI_VFP               80      /* Vertical front porch       */
#endif

/* 5寸720P分辨率的参数配置 */
#if MIPILCD_50_7201280      
#define LCD_DEFAULT_WIDTH       720
#define LCD_DEFAULT_HEIGHT      1280

#define  MIPI_HSYNC             40      /* Horizontal synchronization */
#define  MIPI_HBP               80      /* Horizontal back porch      */
#define  MIPI_HFP               80      /* Horizontal front porch     */
#define  MIPI_VSYNC             60      /* Vertical synchronization   */
#define  MIPI_VBP               70      /* Vertical back porch        */
#define  MIPI_VFP               70      /* Vertical front porch       */
#endif

/* 5.5寸720P分辨率的参数配置 */
#if MIPILCD_55_7201280      
#define LCD_DEFAULT_WIDTH       720
#define LCD_DEFAULT_HEIGHT      1280

#define  MIPI_HSYNC             40      /* Horizontal synchronization */
#define  MIPI_HBP               80      /* Horizontal back porch      */
#define  MIPI_HFP               80      /* Horizontal front porch     */
#define  MIPI_VSYNC             18      /* Vertical synchronization   */
#define  MIPI_VBP               34      /* Vertical back porch        */
#define  MIPI_VFP               26      /* Vertical front porch       */
#endif

/* 5.5寸1080P分辨率的参数配置 */
#if MIPILCD_55_10801920      
#define LCD_DEFAULT_WIDTH       1080
#define LCD_DEFAULT_HEIGHT      1920

#define  MIPI_HSYNC             40      /* Horizontal synchronization */
#define  MIPI_HBP               60      /* Horizontal back porch      */
#define  MIPI_HFP               60      /* Horizontal front porch     */
#define  MIPI_VSYNC             60      /* Vertical synchronization   */
#define  MIPI_VBP               80      /* Vertical back porch        */
#define  MIPI_VFP               80      /* Vertical front porch       */
#endif

/* 7寸1024*600分辨率的参数配置 */
#if MIPILCD_70_1024600      
#define LCD_DEFAULT_WIDTH       1024
#define LCD_DEFAULT_HEIGHT      600

#define  MIPI_HSYNC             20     /* Horizontal synchronization */
#define  MIPI_HBP               160    /* Horizontal back porch      */
#define  MIPI_HFP               160    /* Horizontal front porch     */
#define  MIPI_VSYNC             20     /* Vertical synchronization   */
#define  MIPI_VBP               23     /* Vertical back porch        */
#define  MIPI_VFP               12     /* Vertical front porch       */
#endif

/* 8寸800P分辨率的参数配置 */
#if MIPILCD_80_8001280     
#define LCD_DEFAULT_WIDTH       800
#define LCD_DEFAULT_HEIGHT      1280

#define  MIPI_HSYNC             40     /* Horizontal synchronization */
#define  MIPI_HBP               80     /* Horizontal back porch      */
#define  MIPI_HFP               80     /* Horizontal front porch     */
#define  MIPI_VSYNC             60     /* Vertical synchronization   */
#define  MIPI_VBP               70     /* Vertical back porch        */
#define  MIPI_VFP               70     /* Vertical front porch       */
#endif

/* 10.1寸800P分辨率的参数配置 */
#if MIPILCD_10_8001280     
#define LCD_DEFAULT_WIDTH       800
#define LCD_DEFAULT_HEIGHT      1280

#define  MIPI_HSYNC             40     /* Horizontal synchronization */
#define  MIPI_HBP               80     /* Horizontal back porch      */
#define  MIPI_HFP               80     /* Horizontal front porch     */
#define  MIPI_VSYNC             60     /* Vertical synchronization   */
#define  MIPI_VBP               70     /* Vertical back porch        */
#define  MIPI_VFP               70     /* Vertical front porch       */
#endif


extern DSI_HandleTypeDef g_dsi_handle;       /* DSI句柄 */

/******************************************************************************************/ 
/* DSI LCD 相关引脚 定义 */

#define DSI_BL_GPIO_PORT                GPIOB
#define DSI_BL_GPIO_PIN                 GPIO_PIN_0
#define DSI_BL_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* PB口时钟使能 */

#define DSI_RST_GPIO_PORT               GPIOH
#define DSI_RST_GPIO_PIN                GPIO_PIN_5
#define DSI_RST_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOH_CLK_ENABLE(); }while(0)   /* PH口时钟使能 */


/* DSI背光控制 */
#define DSI_BL(x)            do{ x ? \
                                 HAL_GPIO_WritePin(DSI_BL_GPIO_PORT, DSI_BL_GPIO_PIN, GPIO_PIN_SET) : \
                                 HAL_GPIO_WritePin(DSI_BL_GPIO_PORT, DSI_BL_GPIO_PIN, GPIO_PIN_RESET); \
                             }while(0)

/* DSI复位引脚 */
#define DSI_RST(x)           do{ x ? \
                                 HAL_GPIO_WritePin(DSI_RST_GPIO_PORT, DSI_RST_GPIO_PIN, GPIO_PIN_SET) : \
                                 HAL_GPIO_WritePin(DSI_RST_GPIO_PORT, DSI_RST_GPIO_PIN, GPIO_PIN_RESET); \
                             }while(0)

/******************************************************************************************/
/* 函数声明 */

uint8_t dsi_io_write(uint16_t channelnbr, uint16_t cmd, uint8_t *pdata, uint16_t size);               /* DSI写DCS/Generic指令 */
uint8_t dsi_io_read(uint16_t channelnbr, uint16_t cmd, uint8_t *pdata, uint16_t size);                /* DSI读DCS/Generic指令 */
uint8_t mipilcd_write_cmd(uint16_t channelnbr, uint16_t cmd, const uint8_t *pdata, uint16_t length);  /* 向MIPI LCD写命令 */
       
uint8_t dsi_host_init(void);     /* 初始化DSI HOST */
void dsi_ltdc_init(void);        /* 初始化LTDC */
uint8_t dsi_init(void);          /* 初始化DSI接口 */                                                     

#endif





