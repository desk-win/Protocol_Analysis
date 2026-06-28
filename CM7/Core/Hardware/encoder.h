#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32h7xx_hal.h"

/* ============================== 编码器/旋钮事件 ============================== */
typedef enum {
    ENCODER_EVENT_NONE      = 0,   /* 无事件                          */
    ENCODER_EVENT_CW        = 1,   /* 顺时针旋转一步                   */
    ENCODER_EVENT_CCW       = 2,   /* 逆时针旋转一步                   */
    ENCODER_EVENT_BTN_DOWN  = 3,   /* 按键按下（PE2 低电平）           */
    ENCODER_EVENT_BTN_UP    = 4,   /* 按键释放（PE2 高电平）           */
    ENCODER_EVENT_BTN_LONG  = 5,   /* 长按（约 1 秒）                 */
} EncoderEvent_t;

/* ============================== 公开函数 ============================== */

/**
 * @brief  编码器模块初始化。上电调用一次即可。
 * @note   会重置计数器、按键状态机和事件队列。
 */
void Encoder_Init(void);

/**
 * @brief  获取并消费一个编码器事件（非阻塞）。
 * @return 事件类型；无事件时返回 ENCODER_EVENT_NONE。
 * @note   应用层在主循环或 FreeRTOS 任务中周期性调用，处理返回的事件：
 *         - CW / CCW  → 调节缩放、数值加减等
 *         - BTN_DOWN  → 确认选择
 *         - BTN_UP    → （可选）抬起动作
 *         - BTN_LONG  → 复位 / 返回主菜单等
 */
EncoderEvent_t Encoder_GetEvent(void);

/**
 * @brief  获取累计旋转计数值（顺时针 +1，逆时针 −1）。
 * @note   适用于不需要消费事件的场景，比如直接读取旋转位置。
 */
int32_t Encoder_GetCount(void);

/**
 * @brief  旋转计数清零。
 */
void Encoder_Reset(void);

/**
 * @brief  周期性轮询（主要处理按键消抖状态机）。
 * @note   必须在主循环或任务中以 ≥50Hz 频率调用。
 *         编码器方向判断由 PE3 中断驱动，无需轮询。
 */
void Encoder_Poll(void);

#endif /* __ENCODER_H */
