/**
 ****************************************************************************************************
 * @file        text.h
 * @version     V1.0
 * @brief       汉字显示 代码
 *              提供text_show_font和text_show_string两个函数,用于显示汉字 
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    慧勤智远 STM32开发板
 *
 ****************************************************************************************************
 */

#ifndef __TEXT_H
#define __TEXT_H

#include "./TEXT/fonts.h"


/******************************************************************************************/

/* 接口函数申明 */
static void text_get_hz_mat(uint8_t *code, uint8_t *mat, uint8_t size);   /* 得到汉字的点阵码 */
void text_show_font(uint16_t x, uint16_t y, uint8_t *font, uint8_t size, uint8_t mode, uint32_t color); /* 显示一个指定大小的汉字 */
void text_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, char *str, uint8_t size, uint8_t mode, uint32_t color); /* 在指定位置显示一个字符串 */
void text_show_string_middle(uint16_t x, uint16_t y, char *str, uint8_t size, uint16_t width, uint32_t color); /* 在指定宽度的中间显示字符串 */

#endif





