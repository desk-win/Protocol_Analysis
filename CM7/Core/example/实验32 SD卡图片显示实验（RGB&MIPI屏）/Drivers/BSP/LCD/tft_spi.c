/**
 ****************************************************************************************************
 * @file        tft_spi.c
 * @version     V1.0
 * @brief       TFTLCD(RGB屏) SPI接口 驱动代码
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    STM32H747XIH6小系统板
 *
 ****************************************************************************************************
 */
 
#include "./BSP/LCD/tft_spi.h"
#include "./SYSTEM/delay/delay.h"


/**
 * @brief       SPI写数据
 * @note        向LCD驱动IC写入1 byte数据
 * @param       buf: 要写入的数据
 * @retval      无
 */
void tft_spi_write_byte(uint8_t buf)
{    
    uint8_t count = 0;

    for (count = 0 ; count < 8 ; count++)
    {        
        if (buf & 0x80)       
        {  
            TFT_SPI_SDA(1);   /* 发送1 */
        }
        else                  
        {
            TFT_SPI_SDA(0);   /* 发送0 */
        }
         
        buf <<= 1;
        TFT_SPI_SCL(0);
        delay_us(1);
        TFT_SPI_SCL(1);       /* 上升沿有效 */ 
        delay_us(1);
    }
}

/**
 * @brief       向LCD驱动IC写命令 
 * @param       cmd: 要写入的命令
 * @retval      无
 */
void tft_spi_write_cmd(uint8_t cmd)
{
    TFT_SPI_CS(0);
    TFT_SPI_SDA(0);      /* 写命令 */
    TFT_SPI_SCL(0);
    TFT_SPI_SCL(1);
    delay_us(2);
    tft_spi_write_byte(cmd);
    TFT_SPI_CS(1);
}  

/**
 * @brief       向LCD驱动IC写数据 
 * @param       data: 要写入的数据
 * @retval      无
 */
void tft_spi_write_data(uint8_t data)
{
    TFT_SPI_CS(0);
    TFT_SPI_SDA(1);      /* 写数据 */
    TFT_SPI_SCL(0);
    TFT_SPI_SCL(1);
    delay_us(2);
    tft_spi_write_byte(data);
    TFT_SPI_CS(1);
} 

/**
 * @brief       向LCD驱动IC写寄存器 
 * @param       reg : 寄存器编号
 * @param       data: 要写入的数据
 * @retval      无
 */
void tft_spi_write_reg(uint8_t reg, uint8_t data)
{ 
    tft_spi_write_cmd(reg);       /* 写入要写的寄存器序号/命令 */
    tft_spi_write_data(data);     /* 写入数据 */
}

/**
 * @brief       TFTLCD SPI接口初始化
 * @param       无
 * @retval      无
 */
void tft_spi_init(void)
{    
    GPIO_InitTypeDef gpio_init_struct;

    /* IO 及 时钟配置 */
    TFT_SPI_CS_GPIO_CLK_ENABLE();      /* TFT_SPI_CS引脚时钟使能 */
    TFT_SPI_SCL_GPIO_CLK_ENABLE();     /* TFT_SPI_SCL引脚时钟使能 */
    TFT_SPI_SDA_GPIO_CLK_ENABLE();     /* TFT_SPI_SDA引脚时钟使能 */
    TFT_SPI_RST_GPIO_CLK_ENABLE();     /* TFT_SPI_RST引脚时钟使能 */

    gpio_init_struct.Pin = TFT_SPI_CS_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;                 /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                         /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          /* 高速 */
    HAL_GPIO_Init(TFT_SPI_CS_GPIO_PORT, &gpio_init_struct);      /* 初始化TFT_SPI_CS引脚 */

    gpio_init_struct.Pin = TFT_SPI_SCL_GPIO_PIN;
    HAL_GPIO_Init(TFT_SPI_SCL_GPIO_PORT, &gpio_init_struct);     /* 初始化TFT_SPI_SCL引脚 */  
  
    gpio_init_struct.Pin = TFT_SPI_SDA_GPIO_PIN;
    HAL_GPIO_Init(TFT_SPI_SDA_GPIO_PORT, &gpio_init_struct);     /* 初始化TFT_SPI_SDA引脚 */  
    
    gpio_init_struct.Pin = TFT_SPI_RST_GPIO_PIN;
    HAL_GPIO_Init(TFT_SPI_RST_GPIO_PORT, &gpio_init_struct);     /* 初始化TFT_SPI_RST引脚 */  
        
	  delay_ms(50);
     
    /* LCD复位 */
	  TFT_SPI_RST(1);
	  delay_ms(10);
	  TFT_SPI_RST(0);
	  delay_ms(50);
	  TFT_SPI_RST(1); 
		delay_ms(200); 

//---------------------NV3052CGRB+HSD5.46(HSD055BHW5-C) Initial---
    tft_spi_write_reg(0xFF, 0x30);
    tft_spi_write_reg(0xFF, 0x52);
    tft_spi_write_reg(0xFF, 0x01);
    tft_spi_write_reg(0xE3, 0x00);
    tft_spi_write_reg(0xF6, 0xC0);
    tft_spi_write_reg(0xF0, 0x00);
    tft_spi_write_reg(0x0a, 0x00);  
    
    tft_spi_write_reg(0x23, 0xA2);  //A0   
    
    tft_spi_write_reg(0x24, 0x10);
    tft_spi_write_reg(0x25, 0x0a);
    tft_spi_write_reg(0x26, 0x2E);
    tft_spi_write_reg(0x27, 0x2E);
    tft_spi_write_reg(0x29, 0x04);
    tft_spi_write_reg(0x2A, 0xFF);
    tft_spi_write_reg(0x38, 0x9C);
    tft_spi_write_reg(0x39, 0xA7);
    tft_spi_write_reg(0x3A, 0x5E);  //VCOM  
    tft_spi_write_reg(0x49, 0x3C);
    tft_spi_write_reg(0x91, 0x67);
    tft_spi_write_reg(0x92, 0x67);
    tft_spi_write_reg(0xA0, 0x55);
    tft_spi_write_reg(0xA1, 0x50);
    tft_spi_write_reg(0xA4, 0x9C);
    tft_spi_write_reg(0xA7, 0x02);
    tft_spi_write_reg(0xA8, 0x01);
    tft_spi_write_reg(0xA9, 0x01);
    tft_spi_write_reg(0xAA, 0xFC);
    tft_spi_write_reg(0xAB, 0x28);
    tft_spi_write_reg(0xAC, 0x06);
    tft_spi_write_reg(0xAD, 0x06);
    tft_spi_write_reg(0xAE, 0x06);
    tft_spi_write_reg(0xAF, 0x03);
    tft_spi_write_reg(0xB0, 0x08);
    tft_spi_write_reg(0xB1, 0x26);
    tft_spi_write_reg(0xB2, 0x28);
    tft_spi_write_reg(0xB3, 0x28);
    tft_spi_write_reg(0xB4, 0x03);
    tft_spi_write_reg(0xB5, 0x08);
    tft_spi_write_reg(0xB6, 0x26);
    tft_spi_write_reg(0xB7, 0x08);
    tft_spi_write_reg(0xB8, 0x26);
    tft_spi_write_reg(0xFF, 0x30);
    tft_spi_write_reg(0xFF, 0x52);
    tft_spi_write_reg(0xFF, 0x02);
    tft_spi_write_reg(0xB1, 0x11);
    tft_spi_write_reg(0xD1, 0x15);
    tft_spi_write_reg(0xB4, 0x2F);
    tft_spi_write_reg(0xD4, 0x31);
    tft_spi_write_reg(0xB2, 0x13);
    tft_spi_write_reg(0xD2, 0x15);
    tft_spi_write_reg(0xB3, 0x2D);
    tft_spi_write_reg(0xD3, 0x33);  
    tft_spi_write_reg(0xB6, 0x22);    
    tft_spi_write_reg(0xD6, 0x24);    
    tft_spi_write_reg(0xB7, 0x3D);    
    tft_spi_write_reg(0xD7, 0x41);    
    tft_spi_write_reg(0xC1, 0x08);    
    tft_spi_write_reg(0xE1, 0x08);    
    tft_spi_write_reg(0xB8, 0x0D);    
    tft_spi_write_reg(0xD8, 0x0D);    
    tft_spi_write_reg(0xB9, 0x04);    
    tft_spi_write_reg(0xD9, 0x04);    
    tft_spi_write_reg(0xBD, 0x15);    
    tft_spi_write_reg(0xDD, 0x15);    
    tft_spi_write_reg(0xBC, 0x13);    
    tft_spi_write_reg(0xDC, 0x13);    
    tft_spi_write_reg(0xBB, 0x11);    
    tft_spi_write_reg(0xDB, 0x11);    
    tft_spi_write_reg(0xBA, 0x11);    
    tft_spi_write_reg(0xDA, 0x11);    
    tft_spi_write_reg(0xBE, 0x17);    
    tft_spi_write_reg(0xDE, 0x19);    
    tft_spi_write_reg(0xBF, 0x0F);    
    tft_spi_write_reg(0xDF, 0x11);    
    tft_spi_write_reg(0xC0, 0x16);    
    tft_spi_write_reg(0xE0, 0x18);    
    tft_spi_write_reg(0xB5, 0x37);    
    tft_spi_write_reg(0xD5, 0x34);    
    tft_spi_write_reg(0xB0, 0x02);    
    tft_spi_write_reg(0xD0, 0x05);    
    tft_spi_write_reg(0xFF, 0x30); 
    tft_spi_write_reg(0xFF, 0x52);   
    tft_spi_write_reg(0xFF, 0x03);   
    tft_spi_write_reg(0x05, 0x00);   
    tft_spi_write_reg(0x06, 0x00);   
    tft_spi_write_reg(0x08, 0x06);   
    tft_spi_write_reg(0x09, 0x07);   
    tft_spi_write_reg(0x25, 0x32);   
    tft_spi_write_reg(0x2A, 0x0a);   
    tft_spi_write_reg(0x2B, 0x0b);   
    tft_spi_write_reg(0x70, 0x0f);   
    tft_spi_write_reg(0x71, 0xc0);   
    tft_spi_write_reg(0x30, 0x2A);   
    tft_spi_write_reg(0x31, 0x2A);   
    tft_spi_write_reg(0x32, 0x2A);   
    tft_spi_write_reg(0x33, 0x2A);   
    tft_spi_write_reg(0x34, 0xb1);   
    tft_spi_write_reg(0x35, 0x76);   
    tft_spi_write_reg(0x36, 0x08);   
    tft_spi_write_reg(0x40, 0x07);   
    tft_spi_write_reg(0x41, 0x08);   
    tft_spi_write_reg(0x42, 0x09);   
    tft_spi_write_reg(0x43, 0x0a);   
    tft_spi_write_reg(0x45, 0x04);   
    tft_spi_write_reg(0x46, 0x05);   
    tft_spi_write_reg(0x48, 0x06);   
    tft_spi_write_reg(0x49, 0x07);   
    tft_spi_write_reg(0x50, 0x0b);   
    tft_spi_write_reg(0x51, 0x0c);   
    tft_spi_write_reg(0x52, 0x0d);   
    tft_spi_write_reg(0x53, 0x0e);   
    tft_spi_write_reg(0x55, 0x08);   
    tft_spi_write_reg(0x56, 0x09);   
    tft_spi_write_reg(0x58, 0x0a);   
    tft_spi_write_reg(0x59, 0x0b);   
    tft_spi_write_reg(0x80, 0x1f);   
    tft_spi_write_reg(0x81, 0x00);   
    tft_spi_write_reg(0x82, 0x16);   
    tft_spi_write_reg(0x83, 0x15);   
    tft_spi_write_reg(0x84, 0x0a);   
    tft_spi_write_reg(0x85, 0x0c);   
    tft_spi_write_reg(0x86, 0x0e);   
    tft_spi_write_reg(0x87, 0x10);   
    tft_spi_write_reg(0x88, 0x02);   
    tft_spi_write_reg(0x8F, 0x06);   
    tft_spi_write_reg(0x96, 0x1f);   
    tft_spi_write_reg(0x97, 0x00);   
    tft_spi_write_reg(0x98, 0x18);   
    tft_spi_write_reg(0x99, 0x17);   
    tft_spi_write_reg(0x9A, 0x09);   
    tft_spi_write_reg(0x9B, 0x0b);   
    tft_spi_write_reg(0x9C, 0x0d);   
    tft_spi_write_reg(0x9D, 0x0f);   
    tft_spi_write_reg(0x9E, 0x01);   
    tft_spi_write_reg(0xA5, 0x05);   
    tft_spi_write_reg(0xB0, 0x00);   
    tft_spi_write_reg(0xB1, 0x1f);   
    tft_spi_write_reg(0xB2, 0x16);   
    tft_spi_write_reg(0xB3, 0x15);   
    tft_spi_write_reg(0xB4, 0x0f);   
    tft_spi_write_reg(0xB5, 0x0d);   
    tft_spi_write_reg(0xB6, 0x0b);   
    tft_spi_write_reg(0xB7, 0x09);   
    tft_spi_write_reg(0xB8, 0x05);   
    tft_spi_write_reg(0xBF, 0x01);   
    tft_spi_write_reg(0xC6, 0x00);   
    tft_spi_write_reg(0xC7, 0x1f);   
    tft_spi_write_reg(0xC8, 0x18);   
    tft_spi_write_reg(0xC9, 0x17);   
    tft_spi_write_reg(0xCA, 0x10);   
    tft_spi_write_reg(0xCB, 0x0e);   
    tft_spi_write_reg(0xCC, 0x0c);   
    tft_spi_write_reg(0xCD, 0x0a);   
    tft_spi_write_reg(0xCE, 0x06);   
    tft_spi_write_reg(0xD5, 0x02);   
    tft_spi_write_reg(0xFF, 0x30);   
    tft_spi_write_reg(0xFF, 0x52);   
    tft_spi_write_reg(0xFF, 0x00);   
    tft_spi_write_reg(0x36, 0x0a);   
    tft_spi_write_reg(0x11, 0x00);   
    delay_ms(200); 
    tft_spi_write_reg(0x29, 0x00);      
    delay_ms(100);    
}




