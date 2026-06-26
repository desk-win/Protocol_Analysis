/**
 ****************************************************************************************************
 * @file        piclib.c
 * @version     V1.0
 * @brief       图片解码库 代码 
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    慧勤智远 STM32开发板
 *
 ****************************************************************************************************
 */	
 
#include "./PICTURE/piclib.h"
#include "./BSP/LCD/ltdc.h"


_pic_info picinfo;      /* 图片信息 */
_pic_phy pic_phy;       /* 图片显示物理接口 */


extern uint32_t *g_ltdc_framebuf[2];     /* LTDC LCD帧缓存数组指针,必须指向对应大小的内存区域 */


#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888

/**
 * @brief       颜色填充函数，用于使用ARBG8888格式时的jpeg图片解码填充方式显示
 *              (sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex - sx + 1) * (ey - sy + 1)
 *              注意:sx,ex,不能大于lcddev.width - 1; sy,ey,不能大于lcddev.height - 1
 * @param       sx,sy       : 起始坐标
 * @param       ex,ey       : 结束坐标
 * @param       color       : 填充的颜色数组首地址
 * @retval      无
 */
void jpeg_argb_fill_color(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint32_t psx, psy, pex, pey;    /* 以LCD面板为基准的坐标系,不随横竖屏变化而变化 */
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    /* 坐标系转换 */
    if (lcdltdc.dir)    /* 横屏 */
    {
        psx = sx;
        psy = sy;
        pex = ex;
        pey = ey;
    }
    else                /* 竖屏 */
    {
        psx = sy;
        psy = lcdltdc.pheight - ex - 1;
        pex = ey;
        pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);   /* 行偏移:当前行最后一个像素和下一行第一个像素之间的像素数目 */
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));
    
    RCC->AHB3ENR |= 1 << 4;             /* 使能DMA2D时钟 */
    DMA2D->CR &= ~(1 << 0);             /* 先停止DMA2D */
    DMA2D->CR = 1 << 16;                /* 存储器到存储器并执行PFC模式 */
    DMA2D->FGPFCCR = LTDC_PIXFORMAT_RGB888;    /* 设置前景层颜色格式为RGB888格式(jpeg图片解码数据) */
    DMA2D->OPFCCR = LTDC_PIXFORMAT_ARGB8888;   /* 设置输出颜色格式为ARGB8888格式 */    
    DMA2D->FGOR = 0;                    /* 前景层行偏移为0 */
    DMA2D->OOR = offline;               /* 设置行偏移 */
    DMA2D->FGMAR = (uint32_t)color;     /* 源地址 */
    DMA2D->OMAR = addr;                 /* 输出存储器地址 */
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16); /* 设定行数寄存器 */
    DMA2D->CR |= 1 << 0;                /* 启动DMA2D */

    while ((DMA2D->ISR & (1 << 1)) == 0)/* 等待传输完成 */
    {
        timeout++;

        if (timeout > 0X1FFFFF)break;   /* 超时退出 */
    }

    DMA2D->IFCR |= 1 << 1;              /* 清除传输完成标志 */
}

#endif

/**
 * @brief       填充颜色
 * @param       x, y          : 起始坐标
 * @param       width, height : 宽度和高度
 * @param       color         : 颜色数组
 * @retval      无
 */
static void piclib_fill_color(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *color)
{
    uint16_t i, j;

    if (lcdltdc.pwidth != 0 && lcddev.dir == 0) /* 如果是RGB屏,且竖屏,则填充函数不可直接用 */  
    {  
        for (i = 0; i < height; i++)
        {
            for (j = 0; j < width; j++)
            {
                *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - j - 1) + y + i)) = color[i * width + j];
            }
        }        
    }
    else    /* 其他情况 */
    {
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
        jpeg_argb_fill_color(x, y, x + width - 1, y + height - 1, color);     /* ARGB8888格式填充 */
#else   
        lcd_color_fill(x, y, x + width - 1, y + height - 1, color);           /* 填充 */
#endif
    }    
}

/**
 * @brief       画图初始化
 * @note        在画图之前,必须先调用此函数, 指定相关函数
 * @param       无
 * @retval      无
 */
void piclib_init(void)
{
    pic_phy.read_point = lcd_read_point;    /* 读点函数实现,仅BMP需要 */
    pic_phy.draw_point = lcd_draw_point;    /* 画点函数实现 */
    pic_phy.fill = lcd_fill;                /* 填充函数实现,仅GIF需要 */
    pic_phy.draw_hline = lcd_draw_hline;    /* 画线函数实现,仅GIF需要 */
    pic_phy.fillcolor = piclib_fill_color;  /* 颜色填充函数实现,仅TJPGD需要 */

    picinfo.lcdwidth = lcddev.width;        /* 得到LCD的宽度像素 */
    picinfo.lcdheight = lcddev.height;      /* 得到LCD的高度像素 */

    picinfo.ImgWidth = 0;                   /* 初始化宽度为0 */
    picinfo.ImgHeight = 0;                  /* 初始化高度为0 */
    picinfo.Div_Fac = 0;                    /* 初始化缩放系数为0 */
    picinfo.S_Height = 0;                   /* 初始化设定的高度为0 */
    picinfo.S_Width = 0;                    /* 初始化设定的宽度为0 */
    picinfo.S_XOFF = 0;                     /* 初始化x轴的偏移量为0 */
    picinfo.S_YOFF = 0;                     /* 初始化y轴的偏移量为0 */
    picinfo.staticx = 0;                    /* 初始化当前显示到的x坐标为0 */
    picinfo.staticy = 0;                    /* 初始化当前显示到的y坐标为0 */
}

/**
 * @brief       快速ALPHA BLENDING算法
 * @param       src           : 颜色数
 * @param       dst           : 目标颜色
 * @param       alpha         : 透明程度(0~32)
 * @retval      混合后的颜色
 */
uint16_t piclib_alpha_blend(uint16_t src, uint16_t dst, uint8_t alpha)
{
    uint32_t src2;
    uint32_t dst2;
    /* Convert to 32bit |-----GGGGGG-----RRRRR------BBBBB| */
    src2 = ((src << 16) | src) & 0x07E0F81F;
    dst2 = ((dst << 16) | dst) & 0x07E0F81F;
    /* Perform blending R:G:B with alpha in range 0..32
     * Note that the reason that alpha may not exceed 32 is that there are only
     * 5bits of space between each R:G:B value, any higher value will overflow
     * into the next component and deliver ugly result. 
     */
    dst2 = ((((dst2 - src2) * alpha) >> 5) + src2) & 0x07E0F81F;
    return (dst2 >> 16) | dst2;
}

/**
 * @brief       快速ALPHA BLENDING算法(RGB888颜色)
 * @param       src           : 颜色数
 * @param       dst           : 目标颜色
 * @param       alpha         : 透明程度(0~255)
 * @retval      混合后的颜色
 */
uint32_t piclib_alpha_blend_agb888(uint32_t src, uint32_t dst, uint8_t alpha)
{
    uint16_t r, g, b;
    uint16_t r1, g1, b1;

    b = src & 0xFF; 
    g = (src & 0xFF00) >> 8; 
    r = (src & 0xFF0000) >> 16; 
  
    b1 = dst & 0xFF;  
    g1 = (dst & 0xFF00) >> 8;
    r1 = (dst & 0xFF0000) >> 16;

    b = ((((b1 - b) * alpha) / 255) + b) & 0xFF;  
    g = ((((g1 - g) * alpha) / 255) + g) & 0xFF;
    r = ((((r1 - r) * alpha) / 255) + r) & 0xFF;
  
    return (r << 16) | (g << 8) | b;
  
//    uint64_t src2;
//    uint64_t dst2;
//  
//    /* Convert to 32bit |-----GGGGGG-----RRRRR------BBBBB| */
//    src2 = (((uint64_t)src << 24) | src) & 0xFF00FF00FF;
//    dst2 = (((uint64_t)dst << 24) | dst) & 0xFF00FF00FF;
//    /* Perform blending R:G:B with alpha in range 0..256
//     * Note that the reason that alpha may not exceed 256 is that there are only
//     * 8bits of space between each R:G:B value, any higher value will overflow
//     * into the next component and deliver ugly result. 
//     */
//    dst2 = ((((dst2 - src2) * alpha) >> 8) + src2) & 0xFF00FF00FF;
//    return (dst2 >> 24) | dst2;
}

/**
 * @brief       将RGB565转为RGB888
 * @param       rgb565 : RGB565颜色
 * @retval      RGB888颜色值
 */
uint32_t rgb565torgb888(uint16_t rgb565)
{
    uint16_t r, g, b;
    uint32_t rgb888;
  
    r = (rgb565 & 0XF800) >> 8;
    g = (rgb565 & 0X07E0) >> 3;
    b = (rgb565 & 0X001F) << 3;
  
    rgb888 = (r << 16) | (g << 8) | b;
  
    return rgb888;
}

/**
 * @brief       初始化智能画点
 * @param       无
 * @retval      无
 */
void piclib_ai_draw_init(void)
{
    float temp, temp1;
    temp = (float)picinfo.S_Width / picinfo.ImgWidth;
    temp1 = (float)picinfo.S_Height / picinfo.ImgHeight;

    if (temp < temp1)temp1 = temp;  /* 取较小的那个 */

    if (temp1 > 1)temp1 = 1;

    /* 使图片处于所给区域的中间 */
    picinfo.S_XOFF += (picinfo.S_Width - temp1 * picinfo.ImgWidth) / 2;
    picinfo.S_YOFF += (picinfo.S_Height - temp1 * picinfo.ImgHeight) / 2;
    temp1 *= 8192;                  /* 扩大8192倍 */
    picinfo.Div_Fac = temp1;
    picinfo.staticx = 0xffff;
    picinfo.staticy = 0xffff;       /* 放到一个不可能的值上面 */
}

/**
 * @brief       判断这个像素是否可以显示
 * @param       x, y          : 像素原始坐标
 * @param       chg           : 功能变量
 * @param       无
 * @retval      操作结果
 *   @arg       0   , 不需要显示
 *   @arg       1   , 需要显示
 */
__inline uint8_t piclib_is_element_ok(uint16_t x, uint16_t y, uint8_t chg)
{
    if (x != picinfo.staticx || y != picinfo.staticy)
    {
        if (chg == 1)
        {
            picinfo.staticx = x;
            picinfo.staticy = y;
        }

        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief       智能画图
 * @note        图片仅在x,y和width, height限定的区域内显示.
 *
 * @param       filename      : 包含路径的文件名(.bmp/.jpg/.jpeg/.gif等)
 * @param       x, y          : 起始坐标
 * @param       width, height : 显示区域
 * @param       fast          : 使能快速解码
 *   @arg                       0, 不使能
 *   @arg                       1, 使能
 * @note                        图片尺寸小于等于液晶分辨率,才支持快速解码
 * @retval      操作结果
 *   @arg       0   , 成功
 *   @arg       其他, 错误码
 */
uint8_t piclib_ai_load_picfile(char *filename, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t fast)
{
    uint8_t	res;                                                /* 返回值 */
    uint8_t temp;

    if ((x + width) > picinfo.lcdwidth)return PIC_WINDOW_ERR;   /* x坐标超范围了 */

    if ((y + height) > picinfo.lcdheight)return PIC_WINDOW_ERR; /* y坐标超范围了 */

    /* 得到显示方框大小 */
    if (width == 0 || height == 0)return PIC_WINDOW_ERR;        /* 窗口设定错误 */

    picinfo.S_Height = height;
    picinfo.S_Width = width;

    /* 显示区域无效 */
    if (picinfo.S_Height == 0 || picinfo.S_Width == 0)
    {
        picinfo.S_Height = lcddev.height;
        picinfo.S_Width = lcddev.width;
        return FALSE;
    }

    if (pic_phy.fillcolor == NULL)fast = 0;     /* 颜色填充函数未实现,不能快速显示 */

    /* 显示的开始坐标点 */
    picinfo.S_YOFF = y;
    picinfo.S_XOFF = x;

    /* 文件名传递 */
    temp = exfuns_file_type(filename);          /* 得到文件的类型 */

    switch (temp)
    {
        case T_BMP:
            res = stdbmp_decode(filename);      /* 解码bmp */
            break;

        case T_JPG:
        case T_JPEG:
            res = jpg_decode(filename, fast);   /* 解码JPG/JPEG */
            break;

        case T_GIF:
            res = gif_decode(filename, x, y, width, height);    /* 解码gif */
            break;

        default:
            res = PIC_FORMAT_ERR;               /* 非图片格式!!! */
            break;
    }

    return res;
}

/**
 * @brief       动态分配内存
 * @param       size          : 要申请的内存大小(字节)
 * @retval      分配到的内存首地址
 */
void *piclib_mem_malloc (uint32_t size)
{
    return (void *)mymalloc(SRAMIN, size);
}

/**
 * @brief       释放内存
 * @param       paddr         : 内存首地址
 * @retval      无
 */
void piclib_mem_free (void *paddr)
{
    myfree(SRAMIN, paddr);
}











