/**
 ****************************************************************************************************
 * @file        mipi_lcd.c
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

#include "./BSP/LCD/mipi_lcd.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/LCD/ltdc.h"
#include "./SYSTEM/delay/delay.h"


/* mipi_lcd_ex.c存放各个MIPI LCD驱动IC的初始化部分代码,以简化mipi_lcd.c,该.c文件
 * 不直接加入到工程里面,只有mipi_lcd.c会用到,所以通过include的形式添加.(不要在
 * 其他文件再包含该.c文件!!否则会报错!)	
 */
#include "./BSP/LCD/mipi_lcd_ex.c"


DSI_HandleTypeDef   g_dsi_handle;                  /* DSI句柄 */

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
extern uint32_t ltdc_lcd_framebuf[1080][1920];     /* 定义最大屏分辨率时,LTDC所需的帧缓存数组大小 */
#else
extern uint16_t ltdc_lcd_framebuf[1080][1920];     /* 定义最大屏分辨率时,LTDC所需的帧缓存数组大小 */
//extern uint16_t ltdc_lcd_framebuf1[1080][1920];  /* 使用LTDC层2时才需要（默认使用LTDC层1） */
#endif

extern uint32_t *g_ltdc_framebuf[2];               /* LTDC LCD帧缓存数组指针,必须指向对应大小的内存区域 */


/**
  * @brief  DSI写DCS/Generic指令
  * @param  channelnbr : 虚拟通道ID 
  * @param  cmd        : 要写入的命令
  * @param  pdata      : 数据存储区 
  * @param  size       : 要写入的字节数 
  * @retval 0, 成功; 1, 失败;
  */
uint8_t dsi_io_write(uint16_t channelnbr, uint16_t cmd, uint8_t *pdata, uint16_t size)
{
    uint8_t ret = 0;

    if (size == 0)          /* 写DCS短数据包，不带参数 */
    {
        if (HAL_DSI_ShortWrite(&g_dsi_handle, channelnbr, DSI_DCS_SHORT_PKT_WRITE_P0, cmd, (uint32_t)pdata[0]) != HAL_OK)
        {
            ret = 1;
        }
    }
    else if (size == 1)     /* 写DCS短数据包，带一个参数 */
    {
        if (HAL_DSI_ShortWrite(&g_dsi_handle, channelnbr, DSI_DCS_SHORT_PKT_WRITE_P1, cmd, (uint32_t)pdata[0]) != HAL_OK)
        {
            ret = 1;
        }
    }
    else                    /* 写DCS长数据包 */
    {
        if (HAL_DSI_LongWrite(&g_dsi_handle, channelnbr, DSI_DCS_LONG_PKT_WRITE, size, cmd, pdata) != HAL_OK)
        {
            ret = 1;
        }
    }

    return ret;
}

/**
  * @brief  DSI读DCS/Generic指令
  * @param  channelnbr : 虚拟通道ID 
  * @param  cmd        : 要读取的命令
  * @param  pdata      : 数据存储区 
  * @param  size       : 要读取的字节数 
  * @retval 0, 成功; 1, 失败;
  */
uint8_t dsi_io_read(uint16_t channelnbr, uint16_t cmd, uint8_t *pdata, uint16_t size)
{
    uint8_t ret = 0;     

    /* 读DCS短数据包 */
    if (HAL_DSI_Read(&g_dsi_handle, channelnbr, pdata, size, DSI_DCS_SHORT_PKT_READ, cmd, pdata) != HAL_OK)
    {
        ret = 1;
    }

    return ret;
}

/**
  * @brief  向MIPI LCD驱动IC写命令
  * @param  channelnbr : 虚拟通道ID 
  * @param  cmd        : 要写入的命令
  * @param  pdata      : 数据存储区 
  * @param  size       : 要写入的字节数 
  * @retval 0, 成功; 1, 失败;
  */
uint8_t mipilcd_write_cmd(uint16_t channelnbr, uint16_t cmd, const uint8_t *pdata, uint16_t length)
{
    return dsi_io_write(channelnbr, cmd, (uint8_t *)pdata, length);
}

/**
 * @brief       初始化DSI HOST
 * @param       无
 * @retval      0, 成功; 1, 失败.
 */
uint8_t dsi_host_init(void)
{
    DSI_PLLInitTypeDef pll_init;
    DSI_VidCfgTypeDef vidcfg; 
	  DSI_PHY_TimerTypeDef  phytimings;

    uint32_t dsi_phy_ck;            /* D-PHY时钟频率 */
    uint32_t pixel_clk;             /* LTDC像素时钟频率 */
    uint32_t total_line;            /* 水平总宽度 */
  
    __HAL_RCC_DSI_CLK_ENABLE();     /* 使能DSI时钟 */ 
  
    __HAL_RCC_DSI_FORCE_RESET();    /* 复位DSI */
    delay_ms(100);                  /* 延时100ms */
    __HAL_RCC_DSI_RELEASE_RESET();  /* 结束复位 */
  
    g_dsi_handle.Instance = DSI;                                                  /* DSI */
    g_dsi_handle.Init.AutomaticClockLaneControl = DSI_AUTO_CLK_LANE_CTRL_DISABLE; /* 自动时钟通道控制 */
  
    if (MIPILCD_35_320480)
    {
        g_dsi_handle.Init.TXEscapeCkdiv = 4;                                      /* TX escape clock分频因子为4 */
    }
    else
    {
        g_dsi_handle.Init.TXEscapeCkdiv = 10;                                     /* TX escape clock分频因子为10 */
    }
    
#if MIPILCD_35_320480  
    g_dsi_handle.Init.NumberOfLanes = DSI_ONE_DATA_LANE;                          /* 1路数据线通道 */
#else
    g_dsi_handle.Init.NumberOfLanes = DSI_TWO_DATA_LANES;                         /* 2路数据线通道 */
#endif
    
    /**
     * 配置D-PHY时钟频率，D-PHY时钟用作blclk_ck(DSI byte lane clock)时钟源,
     * D-PHY时钟频率为F_PHY_Mhz = (NDIV * HSE_Mhz) / (IDF * ODF) 
     * 例如:设置NDIV = 24, IDF = 1, ODF = 1, 则dsi_phy_ck时钟频率为:
     * F_PHY = 24 * 25 / (1 * 1) = 600Mhz
     */
    if (MIPILCD_35_320480)
    {
        pll_init.PLLNDIV = 40;                             /* 设置DSI PLL循环分频系数, 取值范围:10~125 */
    }
    else
    {
        pll_init.PLLNDIV = 120;                            /* 设置DSI PLL循环分频系数, 取值范围:10~125 */
    }
   
    pll_init.PLLIDF = DSI_PLL_IN_DIV5;                     /* 设置DSI PLL输入分频系数, 取值范围:1~7 */
    pll_init.PLLODF = DSI_PLL_OUT_DIV1;                    /* 设置DSI PLL输出分频系数, 取值范围:0~3, 对应1/2/4/8分频 */    
    
    if (HAL_DSI_Init(&g_dsi_handle, &pll_init) != HAL_OK)  /* 初始化DSI配置 */
    {
        return 1;
    }
 
    /* Video模式配置 */
    total_line = LCD_DEFAULT_WIDTH + MIPI_HSYNC + MIPI_HBP + MIPI_HFP;

    if (MIPILCD_35_320480)
    {
        dsi_phy_ck = 200000000;
    }
    else
    {
        dsi_phy_ck = 600000000;    
    }    
    
    if (MIPILCD_35_320480)
    {
        pixel_clk = 10000000;
    }
    else if (MIPILCD_70_1024600)
    {
        pixel_clk = 50000000;    
    }
    else if (MIPILCD_55_10801920)
    {
        pixel_clk = 70000000;    
    }
    else
    {
        pixel_clk = 60000000;        
    }
    
    vidcfg.VirtualChannelID = 0;                                   /* 虚拟通道ID */
    vidcfg.ColorCoding = DSI_RGB565;                               /* 设置DPI颜色格式 */
    vidcfg.LooselyPacked = DSI_LOOSELY_PACKED_DISABLE;             /* 不使能Loosely packet(仅在18位颜色格式时使用) */
    vidcfg.Mode = DSI_VID_MODE_BURST;                              /* 设置Video模式的传输方式为突发模式 */
    vidcfg.PacketSize = LCD_DEFAULT_WIDTH;                         /* 设置Video数据包大小 */
    vidcfg.NumberOfChunks = 0;                                     /* 设置在行周期要传输的块数 */
    vidcfg.NullPacketSize = 0xFFFU;                                /* 设置空包大小 */
    vidcfg.HSPolarity = DSI_HSYNC_ACTIVE_LOW;                      /* 水平同步极性:低电平有效 */
    vidcfg.VSPolarity = DSI_VSYNC_ACTIVE_LOW;                      /* 垂直同步极性:低电平有效 */
    vidcfg.DEPolarity = DSI_DATA_ENABLE_ACTIVE_HIGH;               /* 数据使能极性:高电平有效 */
    vidcfg.HorizontalSyncActive = (uint16_t)(MIPI_HSYNC * ((double)dsi_phy_ck / 8 / pixel_clk));  /* 水平同步宽度(在blclk_ck时钟周期下计数) */
    vidcfg.HorizontalBackPorch = (uint16_t)(MIPI_HBP * ((double)dsi_phy_ck / 8 / pixel_clk));     /* 水平后沿宽度(在blclk_ck时钟周期下计数) */
    vidcfg.HorizontalLine = (uint16_t)(total_line * ((double)dsi_phy_ck / 8 / pixel_clk));        /* 水平总宽度(在blclk_ck时钟周期下计数) */
    vidcfg.VerticalSyncActive = MIPI_VSYNC;                        /* 垂直同步高度 */
    vidcfg.VerticalBackPorch = MIPI_VBP;                           /* 垂直后沿高度 */
    vidcfg.VerticalFrontPorch = MIPI_VFP;                          /* 垂直前沿高度 */
    vidcfg.VerticalActive = LCD_DEFAULT_HEIGHT;                    /* 垂直有效高度 */
    vidcfg.LPCommandEnable = DSI_LP_COMMAND_ENABLE;                /* 使能在低功耗模式下的指令传输 */
    vidcfg.LPLargestPacketSize = 4;                                /* 设置低功耗模式下在VSA,VBP和VFP期间传输最大包大小(字节数) */
    vidcfg.LPVACTLargestPacketSize = 4;                            /* 设置低功耗模式下在VACT期间传输最大包大小(字节数) */

    vidcfg.LPHorizontalFrontPorchEnable = DSI_LP_HFP_DISABLE;      /* 禁能在HFP期间内返回低功耗模式 */
    vidcfg.LPHorizontalBackPorchEnable  = DSI_LP_HBP_DISABLE;      /* 禁能在HBP期间内返回低功耗模式 */
    vidcfg.LPVerticalActiveEnable       = DSI_LP_VACT_DISABLE;     /* 禁能在VACT期间内返回低功耗模式 */
    vidcfg.LPVerticalFrontPorchEnable   = DSI_LP_VFP_DISABLE;      /* 禁能在VFP期间内返回低功耗模式 */
    vidcfg.LPVerticalBackPorchEnable    = DSI_LP_VBP_DISABLE;      /* 禁能在VBP期间内返回低功耗模式 */
    vidcfg.LPVerticalSyncActiveEnable   = DSI_LP_VSYNC_DISABLE;    /* 禁能在VSYNC期间内返回低功耗模式 */ 
    vidcfg.FrameBTAAcknowledgeEnable    = DSI_FBTAA_DISABLE;       /* 禁能在一帧结束时请求应答确认 */
    
    if (HAL_DSI_ConfigVideoMode(&g_dsi_handle, &vidcfg) != HAL_OK) /* 配置Video模式 */
    {
        return 1;
    }
 
    /* 配置D-PHY时序 */
    phytimings.ClockLaneHS2LPTime = 35;                  /* D-PHY时钟通道从HS模式转换到LP模式需要的最长时间 */
    phytimings.ClockLaneLP2HSTime = 35;                  /* D-PHY时钟通道从LP模式转换到HS模式需要的最长时间 */
    phytimings.DataLaneHS2LPTime = 35;                   /* D-PHY数据通道从HS模式转换到LP模式需要的最长时间 */
    phytimings.DataLaneLP2HSTime = 35;                   /* D-PHY数据通道从LP模式转换到HS模式需要的最长时间 */
    phytimings.DataLaneMaxReadTime = 0;                  /* 执行一次读取指令所需的最长时间 */
    phytimings.StopWaitTime = 10;                        /* 在停止状态后请求高速传输的最短等待周期 */
    HAL_DSI_ConfigPhyTimer(&g_dsi_handle, &phytimings);  /* 配置D-PHY的时序参数 */
    
    return 0;
}

/**
 * @brief       初始化LTDC
 * @param       无
 * @retval      无
 */
void dsi_ltdc_init(void)
{
    /* LTDC配置 */
    g_ltdc_handle.Instance = LTDC;  
    g_ltdc_handle.Init.HSPolarity = LTDC_HSPOLARITY_AL;      /* 水平同步极性:低电平有效 */
    g_ltdc_handle.Init.VSPolarity = LTDC_VSPOLARITY_AL;      /* 垂直同步极性:低电平有效 */
    g_ltdc_handle.Init.DEPolarity = LTDC_DEPOLARITY_AL;      /* 数据使能极性:低电平有效 */
    g_ltdc_handle.Init.PCPolarity = LTDC_PCPOLARITY_IPC;     /* 像素时钟极性:低电平有效 */
  
    g_ltdc_handle.Init.HorizontalSync = MIPI_HSYNC - 1;                                         /* 水平同步宽度 */
    g_ltdc_handle.Init.VerticalSync = MIPI_VSYNC - 1;                                           /* 垂直同步高度 */
    g_ltdc_handle.Init.AccumulatedHBP = MIPI_HSYNC + MIPI_HBP - 1;                              /* 累加水平后沿宽度 */
    g_ltdc_handle.Init.AccumulatedVBP = MIPI_VSYNC + MIPI_VBP - 1;                              /* 累加垂直后沿高度 */
    g_ltdc_handle.Init.AccumulatedActiveW = MIPI_HSYNC + MIPI_HBP + LCD_DEFAULT_WIDTH - 1;      /* 累加有效宽度 */
    g_ltdc_handle.Init.AccumulatedActiveH = MIPI_VSYNC + MIPI_VBP + LCD_DEFAULT_HEIGHT - 1;     /* 累加有效高度 */
    g_ltdc_handle.Init.TotalWidth = MIPI_HSYNC + MIPI_HBP + LCD_DEFAULT_WIDTH + MIPI_HFP - 1;   /* 总宽度 */
    g_ltdc_handle.Init.TotalHeigh = MIPI_VSYNC + MIPI_VBP + LCD_DEFAULT_HEIGHT + MIPI_VFP - 1;  /* 总高度 */
  
    g_ltdc_handle.Init.Backcolor.Red = 0;                                                       /* 背景色红色值 */
    g_ltdc_handle.Init.Backcolor.Green = 0;                                                     /* 背景色绿色值 */
    g_ltdc_handle.Init.Backcolor.Blue = 0;                                                      /* 背景色蓝色值 */
    HAL_LTDC_Init(&g_ltdc_handle);
  
    /* LTDC参数设置 */
    lcdltdc.pwidth = LCD_DEFAULT_WIDTH;   /* 面板宽度,单位:像素 */    
    lcdltdc.pheight = LCD_DEFAULT_HEIGHT; /* 面板高度,单位:像素 */ 
    lcddev.width = lcdltdc.pwidth;        /* 设置lcddev的宽度参数 */
    lcddev.height = lcdltdc.pheight;      /* 设置lcddev的高度参数 */
    lcdltdc.pixformat = LTDC_PIXFORMAT;   /* 颜色像素格式 */

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
    g_ltdc_framebuf[0] = (uint32_t *)&ltdc_lcd_framebuf;
    lcdltdc.pixsize = 4;    /* 每个像素占4个字节 */
#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    g_ltdc_framebuf[0] = (uint32_t *)&ltdc_lcd_framebuf;
    lcdltdc.pixsize = 3;    /* 每个像素占3个字节 */
#else
    g_ltdc_framebuf[0] = (uint32_t *)&ltdc_lcd_framebuf;
    //g_ltdc_framebuf[1] = (uint32_t*)&ltdc_lcd_framebuf1;
    lcdltdc.pixsize = 2;    /* 每个像素占2个字节 */
#endif

    /* 层配置 */
    ltdc_layer_parameter_config(0, (uint32_t)g_ltdc_framebuf[0], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);   /* 层参数配置 */
    //ltdc_layer_window_config(0, 0, 0, LCD_DEFAULT_WIDTH, LCD_DEFAULT_HEIGHT);                             /* 层窗口配置 */

    ltdc_display_dir(1);                  /* 默认橫屏，在lcd_init函数里面设置 */
    ltdc_select_layer(0);                 /* 选择第1层 */
}

/**
 * @brief       初始化DSI接口
 * @param       无
 * @retval      0, 成功; 1, 失败.
 */
uint8_t dsi_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    
    DSI_BL_GPIO_CLK_ENABLE();         /* DSI_BL引脚时钟使能 */
    DSI_RST_GPIO_CLK_ENABLE();        /* DSI_RST引脚时钟使能 */  
  
    gpio_init_struct.Pin = DSI_BL_GPIO_PIN;                /* 背光控制引脚 */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;           /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                   /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;    /* 高速 */
    HAL_GPIO_Init(DSI_BL_GPIO_PORT, &gpio_init_struct);    /* 初始化DSI_BL引脚 */

    gpio_init_struct.Pin = DSI_RST_GPIO_PIN;               /* 复位引脚 */
    HAL_GPIO_Init(DSI_RST_GPIO_PORT, &gpio_init_struct);   /* 初始化DSI_RST引脚 */
  
    /* MIPI LCD复位 */
    DSI_RST(1);
    delay_ms(20);
    DSI_RST(0);
    delay_ms(50);
    DSI_RST(1); 
    delay_ms(200);
  
    DSI_BL(0);                      /* 关闭背光 */
 
    dsi_host_init();                /* 初始化DSI HOST */
    
    if (MIPILCD_35_320480)
    {
        ltdc_clk_set(300, 25, 30);  /* 设置像素时钟 10Mhz */
    }
    else if (MIPILCD_70_1024600)
    {
        ltdc_clk_set(300, 25, 6);   /* 设置像素时钟 50Mhz */
    }
    else if (MIPILCD_55_10801920)
    {
        ltdc_clk_set(350, 25, 5);   /* 设置像素时钟 70Mhz */
    }
    else
    {
        ltdc_clk_set(300, 25, 5);   /* 设置像素时钟 60Mhz */
    }

    dsi_ltdc_init();                /* 初始化DSI的LTDC */    
    delay_ms(50);
    
    /* 在LTDC初始化后开启DSI模块(为了避免同步问题，DSI应在使能LTDC后再开启) */
    HAL_DSI_Start(&g_dsi_handle);
    
    delay_ms(200);                  /* 延时200ms */
  
    /* 设置MIPI屏驱动IC的ID */
    if (MIPILCD_35_320480)
    {
        lcddev.id = 0X7796;
    }
    else if (MIPILCD_70_1024600)
    {
        lcddev.id = 0X9007;
    }
    else if (MIPILCD_55_7201280)
    {
        lcddev.id = 0X7703;
    }
    else if (MIPILCD_55_10801920)
    {
        lcddev.id = 0X8399;
    }
    else if (MIPILCD_50_7201280 || MIPILCD_80_8001280 || MIPILCD_10_8001280)
    {
        lcddev.id = 0X9881;
    }
    
#if   MIPILCD_35_320480
    mipi_lcd_ex_st7796_cmdinit();
#elif MIPILCD_70_1024600
    mipi_lcd_ex_ek79007_cmdinit();
#elif MIPILCD_50_7201280
    mipi_lcd_ex_ili9881d_cmdinit();
#elif MIPILCD_55_7201280
    mipi_lcd_ex_st7703_cmdinit();
#elif MIPILCD_55_10801920
    mipi_lcd_ex_hx8399_cmdinit();
#elif MIPILCD_80_8001280
    mipi_lcd_ex_ili9881c80_cmdinit();
#elif MIPILCD_10_8001280
    mipi_lcd_ex_ili9881c10_cmdinit();
#endif
    
    return 0;    
}








