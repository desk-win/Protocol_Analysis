/**
 ******************************************************************************
 * @file    encoder.c
 * @brief   旋转编码器（旋钮）+ 按键驱动
 *
 * @硬件连接
 *   PE3  — 编码器 A 相（EXTI 上升沿中断，CubeMX 已配置）
 *   PE4  — 编码器 B 相（普通输入，中断中读取以判断旋转方向）
 *   PE2  — 按键（普通输入，低电平 = 按下，轮询消抖）
 *
 * @消抖策略
 *   - 编码器：时间窗口消抖。A 相上升沿中断后，与上一次有效中断间隔 < 2ms
 *     则判定为抖动丢弃，否则读 B 相电平确定方向并压入事件。
 *   - 按键：状态机 + 时间消抖。30ms 确认状态跳变，1s 判定长按。
 *
 * @调用方式
 *   1. 初始化时调用 Encoder_Init()。
 *   2. 主循环 / FreeRTOS 任务中以 ≥50Hz 调用 Encoder_Poll()（驱动按键状态机）。
 *   3. 调用 Encoder_GetEvent() 消费事件（非阻塞，无事件返回 ENCODER_EVENT_NONE）。
 ******************************************************************************
 */

#include "encoder.h"
#include <stdint.h>

/* ============================== 硬件宏 ============================== */
#define ENC_A_PIN       GPIO_PIN_3      /* PE3 — A 相（中断引脚）               */
#define ENC_B_PIN       GPIO_PIN_4      /* PE4 — B 相（判断方向）               */
#define ENC_BTN_PIN     GPIO_PIN_2      /* PE2 — 按键，低电平按下               */
#define ENC_PORT        GPIOE

/* ============================== 消抖 / 时间参数 (ms) ============================== */
#define ENC_DEBOUNCE_MS       2U        /* 编码器消抖窗口                        */
#define BTN_DEBOUNCE_MS      30U        /* 按键消抖确认时间                      */
#define BTN_LONG_PRESS_MS  1000U        /* 长按判定时间                          */

/* ============================== 事件环形队列 ============================== */
#define EVENT_QUEUE_SIZE     16U

static EncoderEvent_t event_queue[EVENT_QUEUE_SIZE];
static volatile uint8_t event_rd = 0;   /* 读索引（仅主循环访问）               */
static volatile uint8_t event_wr = 0;   /* 写索引（中断中修改）                 */

static volatile int32_t enc_count = 0;  /* 累计旋转计数                        */

/* ============================== 编码器消抖 ============================== */
static volatile uint32_t enc_last_tick = 0;  /* 上次有效中断的时刻 (ms)        */

/* ============================== 按键状态机 ============================== */
typedef enum {
    BTN_STATE_IDLE = 0,          /* 空闲，等待按下                            */
    BTN_STATE_DEBOUNCE_DOWN,     /* 疑似按下，等待消抖确认                     */
    BTN_STATE_PRESSED,           /* 已确认按下                                 */
    BTN_STATE_DEBOUNCE_UP,       /* 疑似释放，等待消抖确认                     */
} BtnState_t;

static BtnState_t btn_state      = BTN_STATE_IDLE;
static uint32_t   btn_last_tick  = 0;      /* 状态切换起始时刻 (ms)            */
static uint32_t   btn_press_start = 0;     /* 按下确认时刻 (ms)                */
static uint8_t    btn_long_sent   = 0;     /* 本次按下是否已发过长按事件       */

/* ============================== 前向声明 ============================== */
static void Encoder_PushEvent(EncoderEvent_t evt);
static void Encoder_ProcessEncIRQ(uint16_t GPIO_Pin);
static void Encoder_ButtonPoll(void);

/* ===================================================================== */
/*                          公开函数                                      */
/* ===================================================================== */

/**
 * @brief  编码器模块初始化。上电调用一次即可。
 */
void Encoder_Init(void)
{
    enc_count       = 0;
    enc_last_tick   = 0;
    btn_state       = BTN_STATE_IDLE;
    btn_last_tick   = 0;
    btn_press_start = 0;
    btn_long_sent   = 0;
    event_rd        = 0;
    event_wr        = 0;

    for (uint8_t i = 0; i < EVENT_QUEUE_SIZE; i++) {
        event_queue[i] = ENCODER_EVENT_NONE;
    }
}

/**
 * @brief  获取并消费一个事件（非阻塞）。
 * @return 事件类型；无事件时返回 ENCODER_EVENT_NONE。
 *
 * 典型用法：
 *   EncoderEvent_t e = Encoder_GetEvent();
 *   if (e == ENCODER_EVENT_CW)       { zoom_in();  }
 *   if (e == ENCODER_EVENT_CCW)      { zoom_out(); }
 *   if (e == ENCODER_EVENT_BTN_DOWN) { confirm();  }
 *   if (e == ENCODER_EVENT_BTN_LONG) { reset();    }
 */
EncoderEvent_t Encoder_GetEvent(void)
{
    if (event_rd == event_wr) {
        return ENCODER_EVENT_NONE;
    }
    EncoderEvent_t evt = event_queue[event_rd];
    event_rd = (event_rd + 1) % EVENT_QUEUE_SIZE;
    return evt;
}

/**
 * @brief  获取累计旋转计数（顺时针 +1，逆时针 −1）。
 */
int32_t Encoder_GetCount(void)
{
    return enc_count;
}

/**
 * @brief  旋转计数清零。
 */
void Encoder_Reset(void)
{
    enc_count = 0;
}

/**
 * @brief  周期性轮询（驱动按键消抖状态机）。
 * @note   必须在主循环或 FreeRTOS 任务中以 ≥50Hz 调用。
 *         编码器方向判断由 PE3 中断驱动，无需此函数介入。
 */
void Encoder_Poll(void)
{
    Encoder_ButtonPoll();
}

/* ===================================================================== */
/*                          内部函数                                      */
/* ===================================================================== */

/**
 * @brief  向事件队列压入一个事件（中断安全）。
 */
static void Encoder_PushEvent(EncoderEvent_t evt)
{
    uint8_t next = (event_wr + 1) % EVENT_QUEUE_SIZE;
    if (next != event_rd) {               /* 队列未满才写入 */
        event_queue[event_wr] = evt;
        event_wr = next;
    }
}

/**
 * @brief  处理编码器 A 相中断：消抖 → 读 B 相判方向 → 压事件。
 */
static void Encoder_ProcessEncIRQ(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != ENC_A_PIN) {
        return;
    }

    uint32_t now = HAL_GetTick();

    /* ---- 消抖：距上次有效中断过近则丢弃 ---- */
    if ((now - enc_last_tick) < ENC_DEBOUNCE_MS) {
        return;
    }
    enc_last_tick = now;

    /* ---- 读 B 相电平判断方向 ----
     * 典型正交编码器时序：
     *   A 相上升沿时 B 相 = 高 → 顺时针 (CW)
     *   A 相上升沿时 B 相 = 低 → 逆时针 (CCW)
     * 注：若实测方向相反，交换下面 CW/CCW 即可。
     */
    if (HAL_GPIO_ReadPin(ENC_PORT, ENC_B_PIN) == GPIO_PIN_SET) {
        enc_count++;
        Encoder_PushEvent(ENCODER_EVENT_CW);
    } else {
        enc_count--;
        Encoder_PushEvent(ENCODER_EVENT_CCW);
    }
}

/**
 * @brief  按键消抖状态机。
 * @note   PE2 低电平 = 按下，高电平 = 释放。
 * PRESSED 期间若持续 ≥1s 且未发过长按事件 → 发送 ENCODER_EVENT_BTN_LONG
 */
static void Encoder_ButtonPoll(void)
{
    uint32_t now = HAL_GetTick();
    /* pin_active: 1 = 按下（低电平），0 = 释放（高电平） */
    uint8_t pin_active = (HAL_GPIO_ReadPin(ENC_PORT, ENC_BTN_PIN) == GPIO_PIN_RESET);

    switch (btn_state) {

    case BTN_STATE_IDLE:
        if (pin_active) {
            btn_state     = BTN_STATE_DEBOUNCE_DOWN;
            btn_last_tick = now;
        }
        break;

    case BTN_STATE_DEBOUNCE_DOWN:
        if (pin_active) {
            if ((now - btn_last_tick) >= BTN_DEBOUNCE_MS) {
                /* 消抖确认：进入按下状态 */
                btn_state       = BTN_STATE_PRESSED;
                btn_press_start = now;
                btn_long_sent   = 0;
                Encoder_PushEvent(ENCODER_EVENT_BTN_DOWN);
            }
        } else {
            /* 消抖期间电平恢复 → 判定为抖动，回到空闲 */
            btn_state = BTN_STATE_IDLE;
        }
        break;

    case BTN_STATE_PRESSED:
        if (!pin_active) {
            /* 电平变高 → 疑似释放 */
            btn_state     = BTN_STATE_DEBOUNCE_UP;
            btn_last_tick = now;
        } else if (!btn_long_sent && ((now - btn_press_start) >= BTN_LONG_PRESS_MS)) {
            /* 持续按住超过 1 秒 → 长按 */
            btn_long_sent = 1;
            Encoder_PushEvent(ENCODER_EVENT_BTN_LONG);
        }
        break;

    case BTN_STATE_DEBOUNCE_UP:
        if (!pin_active) {
            if ((now - btn_last_tick) >= BTN_DEBOUNCE_MS) {
                /* 消抖确认：释放 */
                btn_state = BTN_STATE_IDLE;
                Encoder_PushEvent(ENCODER_EVENT_BTN_UP);
            }
        } else {
            /* 消抖期间电平再次变低 → 抖动，回到按下状态 */
            btn_state = BTN_STATE_PRESSED;
        }
        break;
    }
}

/* ===================================================================== */
/*                      HAL 外部中断回调（覆盖 HAL 库 __weak 定义）         */
/* ===================================================================== */

/**
 * @brief  HAL GPIO EXTI 回调。
 * @note   CM7 目前仅有 PE3 使用 EXTI，故此处分发到编码器处理。
 *         若后续有其他引脚启用 EXTI，需在此函数中按 GPIO_Pin 分发。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    Encoder_ProcessEncIRQ(GPIO_Pin);
}


