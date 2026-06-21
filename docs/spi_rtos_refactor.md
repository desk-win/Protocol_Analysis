# SPI 模块 FreeRTOS 重构方案

> **说明**：本文档展示如何将 `my_spi_check.c` 从裸机模式重构为 FreeRTOS 版本。
> 不是最终代码，而是架构设计 + 关键代码片段，便于理解 RTOS 下的编程模式。
> 其他三个模块（CAN / I2C / UART）的改造思路完全一致。

---

## 〇、前置知识：为什么优先级 0~4 的中断不能调用 FreeRTOS API？

这是理解 RTOS + 中断协作的**核心概念**，必须先讲清楚。

### STM32H7 的中断优先级机制

```
NVIC 优先级寄存器 (8 bit)：
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │
└───┴───┴───┴───┴───┴───┴───┴───┘
  ▲─── 4 bit 优先级 ──▲   ▲── 不使用 ──▲

NVIC_PRIORITYGROUP_4 → 全部 4 位都是抢占优先级，取值 0~15
```

- **数值越小优先级越高**：0 = 最高优先级，15 = 最低优先级
- `HAL_NVIC_SetPriority(IRQn, 5, 0)` → 优先级为 5

### FreeRTOS 临界区保护机制

FreeRTOS 在操作内核数据结构（任务链表、队列、信号量）时，会进入**临界区**来防止数据被破坏：

```c
// FreeRTOS 内部（简化版）
void vTaskSuspendAll(void) {
    // 不是关全局中断！而是设置 BASEPRI 寄存器
    portDISABLE_INTERRUPTS();  // → 写 BASEPRI = 0x50
    // 此时：优先级 5~15 的中断全部被屏蔽
    //       优先级 0~4 的中断仍然可以响应！
    // ... 操作内核数据 ...
    portENABLE_INTERRUPTS();   // → 写 BASEPRI = 0
    // 此时：所有中断恢复正常
}
```

BASEPRI 的计算：
```
configMAX_SYSCALL_INTERRUPT_PRIORITY = 5 << (8-4) = 5 << 4 = 0x50
```

BASEPRI 屏蔽规则：**屏蔽所有优先级值 ≥ BASEPRI 的中断**。

```
中断优先级:  0   1   2   3   4  │  5   6   7   ...  14   15
            ├──不被屏蔽──────────┤  ├──────被 BASEPRI 屏蔽────────┤
            高优先级(紧急)         低优先级(可延迟)
```

### 关键推论

```
┌────────────────────────────────────────────────────────────────┐
│  优先级 0~4 的中断：                                           │
│  ✅ 可以在 FreeRTOS 临界区中触发                                │
│  ✅ 延迟极低（不被任何 RTOS 操作阻塞）                           │
│  ❌ 绝对不能调用任何 FreeRTOS API（包括 xQueueSendFromISR 等）  │
│     原因：FreeRTOS 可能正在临界区修改同一数据结构，导致破坏      │
│                                                                 │
│  优先级 5~15 的中断：                                          │
│  ✅ 可以安全调用 xQueueSendFromISR、xSemaphoreGiveFromISR 等    │
│  ❌ 在 FreeRTOS 临界区期间会被暂时屏蔽（延迟增加）              │
└────────────────────────────────────────────────────────────────┘
```

### 实际例子

```c
// ❌ 错误：FDCAN 中断优先级 = 0，ISR 里调用 FreeRTOS API
// NVIC 优先级 0 不会被 BASEPRI 屏蔽，如果在临界区触发
// → 队列数据结构可能被破坏 → 系统崩溃

// ✅ 正确做法一：优先级设 5，可以安全调用 FromISR API
HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 5, 0);

// ✅ 正确做法二：优先级设 3（极致低延迟），ISR 中只做裸机操作
// 比如：读硬件寄存器、设置全局标志位、然后退出
// 处理逻辑放到一个 FreeRTOS 任务中，由任务轮询标志位
```

### SPI 模块的推荐配置

```
SPI6_IRQn       → 优先级 5（可使用 FromISR API）
EXTI15_10_IRQn  → 优先级 5（CS 引脚中断，可使用 FromISR API）
DMA 相关 IRQ    → 优先级 4 或 5
```

**SPI 的中断本身对延迟不是极度敏感（SPI 时钟通常几十 MHz 以内），优先级 5 完全够用。**

---

## 一、架构总览

### 裸机 vs RTOS 对比

```
裸机模式（现在）                          RTOS 模式（重构后）
─────────────────────────────────        ───────────────────────
                                         
main()                                   SPI_Task (独立任务)
  ├── My_SPI_Init()   // 初始化一次        ├── 初始化阶段
  ├── while(1) {                          │   ├── My_SPI_Init()
  │     if (if_rx_finish) { ... }         │   ├── 创建队列/信号量   
  │     if (if_busy == 0) { ... }         │   └── 注册 ISR 回调
  │   }                                   │
  │                                       ├── 等待命令循环
中断 ISR                                   │   ├── osMessageQueueGet()
  ├── HAL_GPIO_EXTI_Callback              │   │   ├── CMD_SEND
  │     ├── SPI_DMAStop_Manual (重操作!)   │   │   ├── CMD_SEND_RECEIVE
  │     ├── 时间统计                       │   │   ├── CMD_SWITCH_MODE
  │     ├── 错误分析                       │   │   └── CMD_STOP
  │     └── 重启 DMA                       │   │
  ├── HAL_SPI_TxCpltCallback              │   ├── 周期处理
  │     └── CS拉高 + 清 busy              │   │   └── 统计上报
  ├── HAL_SPI_TxRxCpltCallback            │   └── osDelay(1)
  │     ├── 主机：CS拉高 + 清 busy         │
  │     └── 从机：入缓冲区 + 重启 DMA      │  中断 ISR（极简）
  └── HAL_SPI_ErrorCallback              │   ├── CS 边沿 → 发送信号量
      └── 统计错误                         │   ├── TX 完成 → 发送信号量
                                          │   ├── RX 完成 → 发数据队列
全局标志位实现同步：                        │   └── 错误 → 发错误队列
  if_busy      → 忙锁                     │
  if_rx_finish → 接收完成通知              │  RTOS 原语实现同步：
  spi_deploy   → 无保护                   │   信号量 / 消息队列 / 互斥锁
```

### 任务架构图

```
                         ┌──────────────┐
                         │  应用层任务    │
                         │ (用户交互等)   │
                         └──┬───────────┘
                            │ osMessageQueuePut(cmdQ, ...)
                            ▼
                    ┌───────────────┐
                    │   SPI_Task    │  ← 持久任务，优先级 Normal
                    │  (永久循环)    │
                    │               │
                    │  初始化阶段:   │
                    │  My_SPI_Init  │
                    │               │
                    │  运行阶段:     │
                    │  等待命令队列   │
                    │  处理收发请求   │
                    │  管理主从切换   │
                    └──┬──┬──┬──────┘
                       │  │  │
          ┌────────────┘  │  └────────────┐
          ▼               ▼               ▼
    ┌──────────┐   ┌──────────┐   ┌──────────┐
    │ TX 完成   │   │ CS 信号   │   │ RX 数据   │
    │ 信号量    │   │ 信号量    │   │ 消息队列   │
    └──────────┘   └──────────┘   └──────────┘
          ▲               ▲               ▲
          │               │               │
    ┌─────┴─────┐   ┌────┴─────┐   ┌─────┴──────┐
    │ SPI TX ISR│   │EXTI ISR  │   │SPI/DMA ISR │
    │ (HAL回调) │   │(CS引脚)  │   │(HAL回调)   │
    └───────────┘   └──────────┘   └────────────┘
```

---

## 二、使用的 RTOS 原语

| 原语 | CMSIS-RTOS v2 API | 用途 |
|------|-------------------|------|
| **消息队列** | `osMessageQueueNew` / `osMessageQueuePut` / `osMessageQueueGet` | 应用层 → SPI 任务发命令；ISR → 任务递接收数据 |
| **二值信号量** | `osSemaphoreNew(1, 0, ...)` | TX 完成通知、CS 边沿通知 |
| **互斥锁** | `osMutexNew` / `osMutexAcquire` / `osMutexRelease` | 保护 `spi_deploy`、`spi_analyse[]` 等共享结构体 |
| **任务** | `osThreadNew` | SPI 处理任务 |
| **事件标志** | `osThreadFlagsSet` (可选) | CS 上升沿 / 下降沿区分 |

---

## 三、命令与数据定义（新增）

```c
/* ========== 新增：命令与通信结构体 ========== */

// SPI 任务支持的指令类型
typedef enum {
    SPI_CMD_SEND_ONLY,          // 主机：只发不收
    SPI_CMD_SEND_RECEIVE,       // 主机：全双工收发
    SPI_CMD_SWITCH_MODE,        // 切换主从模式
    SPI_CMD_CHANGE_CONFIG,      // 更改参数 (CPOL/CPHA/分频等)
    SPI_CMD_STOP,               // 停止当前操作
    SPI_CMD_GET_ANALYSE,        // 获取协议分析数据
} SPI_CmdType;

// 发送命令结构体
typedef struct {
    SPI_CmdType type;
    uint8_t    *tx_data;        // 发送数据指针
    uint8_t    *rx_data;        // 接收缓冲区指针（CMD_SEND_RECEIVE 时有效）
    uint32_t   len;             // 数据长度
    My_SPI_Mode new_mode;       // CMD_SWITCH_MODE 时有效
} SPI_Cmd;

// ISR 上报给任务的接收结果（从机 DMA 完成时使用）
typedef struct {
    uint32_t data_len;          // 有效数据长度
    uint32_t timestamp;         // 接收完成时刻
    uint8_t  has_error;         // 是否有错误
} SPI_RxReport;

// ISR 上报的错误信息
typedef struct {
    uint32_t error_code;        // HAL ErrorCode 的拷贝
} SPI_ErrorReport;
```

---

## 四、全局 RTOS 对象（替代裸机的全局标志位）

```c
/* ========== RTOS 同步对象（替代裸机标志位） ========== */

// 命令队列：应用层通过此队列给 SPI 任务发指令
// 深度 8，足够缓冲多个待处理命令
static osMessageQueueId_t g_spiCmdQueue;

// 接收数据报告队列：ISR 通过此队列告知任务"DMA 收了多少数据"
static osMessageQueueId_t g_spiRxReportQueue;

// TX 完成信号量：ISR 给信号，任务等待传输结束
static osSemaphoreId_t g_spiTxDoneSem;

// CS 边沿信号量：CS 下降沿或上升沿触发后通知任务
static osSemaphoreId_t g_spiCsEdgeSem;

// 互斥锁：保护 spi_deploy / spi_analyse[] / spi_range_buffer
static osMutexId_t g_spiMutex;

// ── 不再需要的裸机变量 ──
// uint8_t if_busy = 0;              → 替换为 g_spiTxDoneSem
// volatile uint8_t if_rx_finish = 0; → 替换为 g_spiRxReportQueue
```

---

## 五、头文件修改（my_spi_check.h 新增部分）

在原头文件基础上新增以下声明：

```c
/* ========== RTOS 版本新增 ========== */

// SPI 任务入口函数
void SPI_Task(void *argument);

// 应用层调用接口（向 SPI 任务发命令，不直接操作硬件）
HAL_StatusTypeDef SPI_Request_Send(uint8_t *data, uint32_t len, uint32_t timeout_ms);
HAL_StatusTypeDef SPI_Request_SendReceive(uint8_t *tx_data, uint8_t *rx_data,
                                          uint16_t len, uint32_t timeout_ms);
HAL_StatusTypeDef SPI_Request_SwitchMode(My_SPI_Mode mode);
HAL_StatusTypeDef SPI_Request_ChangeConfig(uint8_t time_mode, My_SPI_Mode role,
                                           uint8_t cs_delay, uint16_t baudrate,
                                           uint8_t cs_polarity);

// 协议分析数据获取（带互斥保护）
HAL_StatusTypeDef SPI_GetAnalyseData(My_SPI_Analyse *dest, uint8_t index);

// 环形缓冲区读取（带互斥保护）
uint8_t SPI_ReadRingBuffer_Safe(uint8_t *data, uint32_t data_len);
```

---

## 六、源文件核心改造（逐段对比）

### 6.1 工具函数 —— 加互斥锁保护环形缓冲区

```c
/* ========== 原函数不变，新增带锁的包装版本 ========== */

/**
 * @brief 线程安全的环形缓冲区写入
 * @note  仅在任务上下文调用（ISR 应该直接发队列，不写环形缓冲区）
 */
uint8_t SPI_RangeBuffer_Write_Safe(uint8_t data) {
    uint8_t result;
    osMutexAcquire(g_spiMutex, osWaitForever);
    result = SPI_RangeBuffer_Wirte(data);   // 调用原函数
    osMutexRelease(g_spiMutex);
    return result;
}

/**
 * @brief 线程安全的环形缓冲区读取
 */
uint8_t SPI_RangeBuffer_Read_Safe(uint8_t *data, uint32_t data_len) {
    uint8_t result;
    osMutexAcquire(g_spiMutex, osWaitForever);
    result = SPI_RangeBuffer_Read(data, data_len);  // 调用原函数
    osMutexRelease(g_spiMutex);
    return result;
}
```

### 6.2 初始化和配置函数 —— 基本不变

`My_SPI_Init()`、`SPI_Time_Mode()`、`SPI_BaudRatePrescaler()`、`CS_Switch_To_Output()`、`CS_Switch_To_Exit()`、`Switch_SPI_Mode()` 这些函数**不需要大改**，它们只操作 HAL 句柄和 GPIO，在任务上下文中调用是安全的。

唯一需要注意的是：**在调用这些函数前后加互斥锁**，防止和统计读取任务冲突：

```c
void My_SPI_Init_Safe(uint8_t time_mode, My_SPI_Mode role, uint8_t cs_delay,
                       uint16_t baudrate, uint8_t cs_polarity) {
    osMutexAcquire(g_spiMutex, osWaitForever);
    My_SPI_Init(time_mode, role, cs_delay, baudrate, cs_polarity);  // 原函数
    osMutexRelease(g_spiMutex);
}
```

### 6.3 主机模式 —— 从"忙标志位"切换到"信号量等待"

**【原裸机代码】**

```c
// 原代码：用 if_busy 做互斥，HAL_SPI_Transmit_IT 异步发送
void My_SPI_Send(uint8_t *data, uint32_t len) {
    if (spi_deploy.spi_role == MY_SPI_MASTER) {
        if (if_busy == 0) {           // ← 忙标志位
            if_busy = 1;
            CS_Pin_State(0);
            HAL_SPI_Transmit_IT(&hspi6, data, len);
            // 调用者不知道什么时候发完，只能轮询 if_busy
        }
    }
}

// TX 完成 ISR 释放忙标志
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == hspi6.Instance) {
        CS_Pin_State(1);
        if_busy = 0;                  // ← 释放
    }
}
```

**【RTOS 重构代码】**

```c
/**
 * @brief SPI 任务中处理主机发送
 * @note  在 SPI_Task 的命令循环中调用
 */
static HAL_StatusTypeDef SPI_Master_Send(uint8_t *data, uint32_t len) {
    // 检查是否是主机模式
    if (spi_deploy.spi_role != MY_SPI_MASTER) {
        return HAL_ERROR;
    }

    // 拉低 CS
    if (spi_deploy.cs_polarity == 0) CS_Pin_State(0);
    else CS_Pin_State(1);

    // 启动中断发送
    HAL_StatusTypeDef status = HAL_SPI_Transmit_IT(&hspi6, data, len);
    if (status != HAL_OK) {
        // 发送启动失败，拉高 CS
        if (spi_deploy.cs_polarity == 0) CS_Pin_State(1);
        else CS_Pin_State(0);
        return status;
    }

    // ── 关键区别：阻塞等待信号量，不忙等 ──
    // 等待 TX 完成 ISR 发送信号量，超时 1000ms
    if (osSemaphoreAcquire(g_spiTxDoneSem, 1000) != osOK) {
        // 超时！可能是 SPI 硬件错误
        SPI_DMAStop_Manual(&hspi6);  // 强制停止
        CS_Pin_State(1);
        return HAL_TIMEOUT;
    }

    // ISR 已经把 CS 拉高了，这里直接返回成功
    return HAL_OK;
}

/**
 * @brief 主机全双工收发
 */
static HAL_StatusTypeDef SPI_Master_SendReceive(uint8_t *tx_data, uint8_t *rx_data,
                                                  uint16_t len) {
    if (spi_deploy.spi_role != MY_SPI_MASTER) return HAL_ERROR;

    if (spi_deploy.cs_polarity == 0) CS_Pin_State(0);
    else CS_Pin_State(1);

    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_IT(&hspi6, tx_data, rx_data, len);
    if (status != HAL_OK) {
        CS_Pin_State(1);
        return status;
    }

    // 等待完成
    if (osSemaphoreAcquire(g_spiTxDoneSem, 1000) != osOK) {
        SPI_DMAStop_Manual(&hspi6);
        CS_Pin_State(1);
        return HAL_TIMEOUT;
    }

    return HAL_OK;
}
```

### 6.4 ISR 回调 —— 极简化（核心改造）

改造原则：**ISR 只做两件事——收数据和发信号，逻辑全部移到任务中。**

**【原代码】** ISR 里做了大量操作（停止 DMA、统计时间、计算成功率、重启 DMA）

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_10) {
        if (spi_deploy.spi_role == MY_SPI_SLAVE) {
            if (HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_10) == GPIO_PIN_RESET) {
                analyse_index++;                                    // ← 数组操作
                spi_analyse[analyse_index].cs_low_tick = Get_Sys_us(); // ← 时间统计
            } else if(...) { 
                spi_analyse[analyse_index].cs_hight_tick = Get_Sys_us();
                SPI_DMAStop_Manual(&hspi6);           // ← 重操作！关闭 DMA/SPI
                // ... 十几行统计分析 ...              // ← 全在 ISR 里!
                if_rx_finish = 1;
                SPI_PutData_To_Buffer();              // ← 写环形缓冲区
                HAL_SPI_TransmitReceive_DMA(...);     // ← 重启 DMA
            }
        }
    }
}
```

问题：ISR 执行时间过长 → 阻塞其他中断 → 可能导致 CAN 丢帧、UART 溢出等。

**【RTOS 重构代码】**

```c
/**
 * @brief CS 引脚外部中断回调（极简版本）
 * @note  只记录时间 + 发信号量，所有逻辑移到 SPI_Task
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin != GPIO_PIN_10) return;
    if (spi_deploy.spi_role != MY_SPI_SLAVE) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t now_tick = Get_Sys_us();
    uint8_t cs_level = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_10);

    if (cs_level == GPIO_PIN_RESET) {
        // CS 下降沿：只记录时间
        g_cs_fall_tick = now_tick;               // 全局变量（原子写入 uint32_t）
    } else {
        // CS 上升沿：记录时间 + 通知任务
        g_cs_rise_tick = now_tick;               // 全局变量
        osSemaphoreRelease(g_spiCsEdgeSem);     // ← 唤醒 SPI_Task（FromISR 安全）
    }

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_10);       // 清中断标志
    portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief SPI TxRx 完成回调（极简版本）
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance != hspi6.Instance) return;

    if (spi_deploy.spi_role == MY_SPI_MASTER) {
        // 主机：拉高 CS + 通知任务传输完成
        if (spi_deploy.cs_polarity == 0) CS_Pin_State(1);
        else CS_Pin_State(0);
        osSemaphoreRelease(g_spiTxDoneSem);      // ← 唤醒等待的任务
    }
    // 从机模式：DMA 收满一轮，通知任务
    else if (spi_deploy.spi_role == MY_SPI_SLAVE) {
        SPI_RxReport report = {
            .data_len  = DMA_BUFFER_LEN,
            .timestamp = Get_Sys_us(),
            .has_error  = 0,
        };
        osMessageQueuePut(g_spiRxReportQueue, &report, 0, 0);  // ← 发数据队列
    }
}

/**
 * @brief SPI TX 完成回调
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance != hspi6.Instance) return;
    if (spi_deploy.spi_role == MY_SPI_MASTER) {
        if (spi_deploy.cs_polarity == 0) CS_Pin_State(1);
        else CS_Pin_State(0);
        osSemaphoreRelease(g_spiTxDoneSem);
    }
}

/**
 * @brief SPI 错误回调（极简：只发错误通知，不处理逻辑）
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance != hspi6.Instance) return;

    SPI_ErrorReport err = {
        .error_code = hspi->ErrorCode,
    };
    osMessageQueuePut(g_spiRxReportQueue, &err, 0, 0);  // 复用报告队列
    // 错误统计移到任务里做
}
```

---

### 6.5 从机模式处理 —— 全部逻辑移到任务中

```c
/**
 * @brief SPI 任务中处理从机一次完整帧（CS 下降沿→上升沿）
 * @note  由 CS 信号量唤醒后执行
 */
static void SPI_Slave_ProcessFrame(void) {
    uint32_t cs_low  = g_cs_fall_tick;    // ISR 记录的时间
    uint32_t cs_high = g_cs_rise_tick;

    // ── 以下全部在任务上下文，可以用 osDelay，可以花时间计算 ──

    // 1. 停止 DMA 接收
    SPI_DMAStop_Manual(&hspi6);

    // 2. 计算本次数据长度
    uint32_t data_len = DMA_BUFFER_LEN - __HAL_DMA_GET_COUNTER(&hdma_spi6_rx);

    // 3. 将数据写入环形缓冲区
    for (uint32_t i = 0; i < data_len; i++) {
        SPI_RangeBuffer_Write_Safe(rx_spi_buffer[i]);
    }

    // 4. 协议分析（保护）
    osMutexAcquire(g_spiMutex, osWaitForever);
    {
        analyse_index++;
        spi_analyse[analyse_index].cs_low_tick  = cs_low;
        spi_analyse[analyse_index].cs_hight_tick = cs_high;
        spi_analyse[analyse_index].cs_gap_tick   = cs_high - cs_low;

        spi_analyse[analyse_index].spi_total_frame = analyse_index;
        if (spi_deploy.spi_error.error_code == 0) {
            spi_analyse[0].spi_success_frame++;
            spi_analyse[analyse_index].spi_success_frame = spi_analyse[0].spi_success_frame;
        } else {
            spi_analyse[0].spi_fail_frame++;
            spi_analyse[analyse_index].spi_fail_frame = spi_analyse[0].spi_fail_frame;
        }

        if (spi_analyse[analyse_index].spi_total_frame > 1) {
            spi_analyse[analyse_index].spi_frame_gap =
                spi_analyse[analyse_index].cs_low_tick -
                spi_analyse[analyse_index - 1].cs_hight_tick;
        }

        uint32_t total = spi_analyse[0].spi_success_frame + spi_analyse[0].spi_fail_frame;
        if (total > 0) {
            spi_analyse[analyse_index].transmit_success_rate =
                (float_t)spi_analyse[0].spi_success_frame / total;
        }
    }
    osMutexRelease(g_spiMutex);

    // 5. 重新启动 DMA 接收（准备接收下一帧）
    HAL_SPI_TransmitReceive_DMA(&hspi6, tx_spi_buffer, rx_spi_buffer, DMA_BUFFER_LEN);
}

/**
 * @brief 处理 DMA 收满一轮的情况（从机长时间接收时，DMA 循环溢出）
 */
static void SPI_Slave_ProcessDMABuffer(void) {
    // DMA BUFFER 满了，数据已经在 rx_spi_buffer 里
    for (uint32_t i = 0; i < DMA_BUFFER_LEN; i++) {
        SPI_RangeBuffer_Write_Safe(rx_spi_buffer[i]);
    }
    // 重新启动 DMA
    HAL_SPI_TransmitReceive_DMA(&hspi6, tx_spi_buffer, rx_spi_buffer, DMA_BUFFER_LEN);
}
```

---

### 6.6 SPI 任务主函数（核心）

```c
/**
 * @brief SPI 协议处理任务
 *
 * 架构：
 *   1. 初始化阶段（运行一次）
 *   2. 命令循环（永久）
 *      - 等待命令队列 (应用层请求)
 *      - 等待 CS 信号量 (从机模式帧边界)
 *      - 等待 RX 报告队列 (DMA 完成 / 错误)
 *
 * @note  任务栈大小建议 512 bytes
 * @note  优先级建议 osPriorityNormal (=24)
 */
void SPI_Task(void *argument) {
    // ==================== 第一阶段：创建同步对象 ====================
    g_spiCmdQueue     = osMessageQueueNew(8, sizeof(SPI_Cmd), NULL);
    g_spiRxReportQueue = osMessageQueueNew(4, sizeof(SPI_RxReport), NULL);
    g_spiTxDoneSem    = osSemaphoreNew(1, 0, NULL);   // 初始 count=0, max=1 (二值)
    g_spiCsEdgeSem    = osSemaphoreNew(1, 0, NULL);
    g_spiMutex        = osMutexNew(NULL);

    // ==================== 第二阶段：等待初始化命令 ====================
    // 任务创建后不立即初始化 SPI，而是等待应用层发来配置命令
    SPI_Cmd init_cmd;
    osMessageQueueGet(g_spiCmdQueue, &init_cmd, NULL, osWaitForever);

    if (init_cmd.type == SPI_CMD_CHANGE_CONFIG) {
        // My_SPI_Init 内部会调用 HAL_SPI_Init、配置 GPIO 等
        My_SPI_Init(0, MY_SPI_SLAVE, 0, 256, 0);  // 默认配置，由命令参数覆盖
    }
    // 初始默认从机模式（或根据用户配置）
    // 实际项目中应该从 init_cmd 中解析参数

    // ==================== 第三阶段：主命令循环 ====================
    for (;;) {
        SPI_Cmd cmd;
        SPI_RxReport rx_report;

        // ── 同时等待三类事件（使用 0 超时轮询，或 osWaitForever 阻塞） ──

        // ① 检查命令队列
        if (osMessageQueueGet(g_spiCmdQueue, &cmd, NULL, 0) == osOK) {
            switch (cmd.type) {

            case SPI_CMD_SEND_ONLY:
                SPI_Master_Send(cmd.tx_data, cmd.len);
                break;

            case SPI_CMD_SEND_RECEIVE:
                SPI_Master_SendReceive(cmd.tx_data, cmd.rx_data, cmd.len);
                break;

            case SPI_CMD_SWITCH_MODE:
                osMutexAcquire(g_spiMutex, osWaitForever);
                Switch_SPI_Mode(cmd.new_mode);
                osMutexRelease(g_spiMutex);
                break;

            case SPI_CMD_CHANGE_CONFIG:
                osMutexAcquire(g_spiMutex, osWaitForever);
                // 从 cmd 中解析配置参数...
                osMutexRelease(g_spiMutex);
                break;

            case SPI_CMD_STOP:
                // 停止 DMA，清理状态
                SPI_DMAStop_Manual(&hspi6);
                break;

            default:
                break;
            }
        }

        // ② 检查从机模式 CS 信号量（非阻塞）
        if (osSemaphoreAcquire(g_spiCsEdgeSem, 0) == osOK) {
            SPI_Slave_ProcessFrame();
        }

        // ③ 检查 RX 报告队列（DMA 完成 / 错误报告）
        if (osMessageQueueGet(g_spiRxReportQueue, &rx_report, NULL, 0) == osOK) {
            if (rx_report.has_error) {
                // 处理错误：可以在这里统计、告警、自动重试
            } else {
                // DMA 收满一轮
                SPI_Slave_ProcessDMABuffer();
            }
        }

        // 让出 CPU，避免死循环占满内核
        osDelay(1);
    }
}
```

---

### 6.7 应用层调用接口（对外的 API）

```c
/**
 * @brief 请求 SPI 发送数据（主机模式）
 * @param data       发送数据
 * @param len        长度
 * @param timeout_ms 超时时间（等待 SPI 任务处理完成）
 * @return HAL_OK / HAL_TIMEOUT / HAL_ERROR
 *
 * @note  可以在任何任务中调用
 *        内部通过消息队列发送命令，然后阻塞等待完成信号量
 */
HAL_StatusTypeDef SPI_Request_Send(uint8_t *data, uint32_t len, uint32_t timeout_ms) {
    SPI_Cmd cmd = {
        .type    = SPI_CMD_SEND_ONLY,
        .tx_data = data,
        .len     = len,
    };

    // 将命令发给 SPI 任务（带超时，防止队列满时永久阻塞）
    osStatus_t ret = osMessageQueuePut(g_spiCmdQueue, &cmd, 0, timeout_ms);
    if (ret != osOK) {
        return HAL_ERROR;   // 队列满或超时
    }

    // 不需要在这里等完成——SPI_Task 自己会等 TX 完成信号量
    // 如果调用者需要知道结果，可以再加一个"响应队列"或"完成信号量"
    return HAL_OK;
}

/**
 * @brief 请求切换主从模式
 */
HAL_StatusTypeDef SPI_Request_SwitchMode(My_SPI_Mode mode) {
    SPI_Cmd cmd = {
        .type     = SPI_CMD_SWITCH_MODE,
        .new_mode = mode,
    };
    return (osMessageQueuePut(g_spiCmdQueue, &cmd, 0, 100) == osOK)
           ? HAL_OK : HAL_ERROR;
}

/**
 * @brief 获取协议分析数据（线程安全）
 */
HAL_StatusTypeDef SPI_GetAnalyseData(My_SPI_Analyse *dest, uint8_t index) {
    if (osMutexAcquire(g_spiMutex, 100) != osOK) return HAL_TIMEOUT;
    memcpy(dest, &spi_analyse[index], sizeof(My_SPI_Analyse));
    osMutexRelease(g_spiMutex);
    return HAL_OK;
}
```

---

## 七、系统初始化（freertos.c 中创建任务）

```c
/* ========== freertos.c 中添加 ========== */

// 在 MX_FREERTOS_Init() 中添加：
osThreadId_t spiTaskHandle;
const osThreadAttr_t spiTask_attributes = {
    .name       = "SPI_Task",
    .stack_size = 512 * 4,           // 2KB 栈（SPI 分析结构体较大）
    .priority   = osPriorityNormal,  // 优先级 24
};

void MX_FREERTOS_Init(void) {
    // ... 原有的 defaultTask ...

    /* USER CODE BEGIN RTOS_THREADS */
    spiTaskHandle = osThreadNew(SPI_Task, NULL, &spiTask_attributes);
    /* USER CODE END RTOS_THREADS */
}
```

---

## 八、裸机 vs RTOS 对比总结

| 维度 | 裸机原代码 | RTOS 重构后 |
|------|-----------|------------|
| **ISR 执行时间** | 长（几十行逻辑在 ISR 里） | 短（~5 行，只发信号/队列） |
| **发送同步** | 轮询 `if_busy` 忙等 | 信号量阻塞，CPU 让给其他任务 |
| **数据竞争** | `spi_analyse[]` 无保护 | 互斥锁保护，安全读写 |
| **CS 帧处理** | ISR 里做全部分析 | 任务中从容处理，可加复杂逻辑 |
| **超时处理** | 无超时（死等） | `osSemaphoreAcquire(timeout)` 超时恢复 |
| **错误恢复** | ISR 里简单统计 | 任务中可根据错误类型自动重试/切换模式 |
| **CPU 利用** | ISR + main 循环，忙等多 | 信号量驱动，无事时 CPU 休眠（idle task） |
| **可测试性** | 难以单独测试 ISR | 任务可独立测试，ISR 只是薄层 |

---

## 九、其他三个模块的改造要点（一致性对照）

四个模块的改造模式完全统一，差异仅在于：

| 模块 | CS 信号量源 | 数据入环方式 | 特殊点 |
|------|-----------|-------------|--------|
| **CAN** | FDCAN RX FIFO 中断 | ISR 直接发消息队列（不经过环形缓冲区），任务中写入环形缓冲区 | `CAN_Statis_Task` 做成独立定时任务或用 `osDelay(1000)` |
| **I2C** | 地址匹配中断 + 停止位中断 | 同 CAN，ISR 发队列 → 任务写环形缓冲区 | `My_I2C_Bus_Check` 独立为最低优先级任务，每 100ms 检查一次；`My_I2C_Master_ScanBus` 在任务中用 `osDelay` 替代 `HAL_Delay` |
| **UART** | IDLE 空闲中断 | ISR 直接写环形缓冲区（单生产者），任务读取 | `My_UART_LoopTask` 变成 UART_Task 内的循环；循环发送用 `osDelay(interval_ms)` |

**核心原则始终一致**：
1. ISR 只做数据搬运 + 信号量/队列通知
2. 所有分析、统计、错误处理逻辑在任务中
3. 共享数据结构加互斥锁
4. 耗时等待（`HAL_Delay`）替换为 `osDelay`

---

## 十、迁移建议

不需要一次性全部重构，建议按以下步骤渐进：

```
第一步：先改 SPI（最简单）
  ├── 创建 SPI_Task
  ├── 改造 ISR（只发信号量）
  ├── 把主循环的 SPI 逻辑移到任务
  └── 验证：一样能正常收发

第二步：改 UART
  ├── 模式几乎与 SPI 一致
  └── 验证：IDLE 中断 + 循环发送

第三步：改 CAN（实时性要求最高）
  ├── FDCAN 中断优先级提到 3
  ├── ISR 发数据到消息队列
  └── 验证：高负载下不丢帧

第四步：改 I2C（最复杂，有从机监听模式）
  ├── 从机监听 ISR → 任务处理
  ├── 总线死锁检查独立为低优先级任务
  └── 验证：主从模式都正常
```
