# CM4 架构说明

## 概述

CM4 负责**数据采集**和**协议通信**（UART/SPI/I2C/CAN），CM7 负责 UI 交互和显示。两个核通过共享内存（D2 SRAM1）+ 硬件信号量（HSEM）通信。

## 一、任务架构

```
FreeRTOS Tasks (共 7 个)

Proto_Select        (osPriorityAboveNormal)  — 协议调度器，等 M7 通知，创建/销毁协议 Worker
UART_Callback_Task  (osPriorityNormal)       — UART 收发处理（按需创建）
SPI_Callback_Task   (osPriorityNormal)       — SPI 收发处理（按需创建）
I2C_Callback_Task   (osPriorityNormal)       — I2C 收发处理（按需创建）
CAN_Callback_Task   (osPriorityNormal)       — CAN 收发处理（按需创建）
Control_DCMI_Task   (osPriorityLow)          — DCMI 数据采集启停控制
DMA_Catch_Ctrl_Task (osPriorityLow)          — DMA_Catch 逻辑分析仪启停控制
defaultTask         (osPriorityNormal)       — 空闲（CubeMX 模板）
```

**关键设计：**
- 四种协议**一次只跑一个**，`Proto_Select` 只维护一个活跃的协议 Task
- DCMI 和 DMA_Catch 独立于协议，始终可运行，由 M7 通过配置位独立控制
- ISR 只做轻活（记标志 + Give 信号量），重活全在 Task 里

## 二、双核通信机制

### 共享内存布局（D2 SRAM1 @ 0x30000000，128KB）

```
0x30000000  SHM_RING        2 KB    M4→M7 环形缓冲区（协议数据流）
0x30001000  SHM_CONFIG     64 B    M7→M4 协议配置（proto_config_t）
0x30001040  SHM_STATUS     32 B    M4→M7 状态标志（protocol_done/error）
0x30001060  SHM_TX_BUF    258 B    M7→M4 发送数据缓冲区 + 长度
0x30002000  DCMI_ADC_A   3840 B    DCMI ADC-A 通道解析结果（CM7 直接读）
0x30002F00  DCMI_ADC_B   3840 B    DCMI ADC-B 通道解析结果
0x30003E00  DCMI_CTRL      16 B    DCMI 控制块
0x30004000  DMA_CATCH_BUF  40 KB   DMA_Catch 波形数据（CM7 直接读）
0x3000DC40  DMA_CATCH_CTRL  8 B    DMA_Catch 控制块
```

### HSEM 分配

| ID | 方向 | 用途 |
|----|------|------|
| 0 | CM7→CM4 | 上电 boot 同步（一次性） |
| 1 | CM7→CM4 | 配置变更通知 |
| 2 | CM4→CM7 | 协议完成通知 |

### 配置流程

```
1. CM7 用户操作 UI → 写 SHM_CONFIG + SHM_TX_BUF
2. CM7 调 shm_config_notify() → HSEM ID=1 Release
3. CM4 HSEM2_IRQ → xSemaphoreGiveFromISR(hsem_config_sem)
4. Proto_Select 被唤醒 → 杀旧协议 Task → apply 新配置 → 创建新协议 Task
```

### 协议完成后通知 CM7

```
协议 Task 发送/接收完成
  → SHM_STATUS->protocol_done = 1
  → HSEM ID=2 Release → CM7 中断 → UI 更新
```

## 三、外设中断优先级

CM4 NVIC 优先级（4-bit，数字越小优先级越高）：

```
优先级 0-4   禁止调用 FreeRTOS API（不用）
优先级 5     所有用户外设 ISR（DMA/UART/SPI/I2C/CAN/HSEM/DCMI）
优先级 15    FreeRTOS 内核（PendSV/SysTick）
```

`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`，只有优先级 ≥5 的 ISR 才能调用 `xSemaphoreGiveFromISR`。

## 四、协议 ISR→Task 数据流模板

所有四个协议统一采用此模式：

```
ISR 回调（几微秒）:
  ├── 数据写入本地环形缓冲区
  ├── 重启外设/DMA（如果需要）
  └── xSemaphoreGiveFromISR(sem) → 通知 Task

Task（不限时）:
  ├── xSemaphoreTake(sem) 阻塞等待
  ├── 帧分析 / 错误统计
  ├── shm_push*() 推入 SHM_RING → CM7 实时消费
  └── 循环
```

## 五、CM7 如何与 CM4 交互

### 发送数据（主机模式）

```c
// CM7 侧
memcpy((void*)SHM_TX_BUF, data, len);
*SHM_TX_LEN = len;
__DMB();
// ... 写 SHM_CONFIG ...
shm_config_notify();  // HSEM ID=1

// CM4 协议 Task 自动检测 SHM_TX_LEN > 0 → 发送 → 完成 → HSEM ID=2 通知 CM7
```

### 读取接收数据

```c
// CM7 侧（轮询，如前每 100ms）
SCB_InvalidateDCache_by_Addr((uint32_t*)0x30000000, 2048);
uint8_t buf[64];
uint16_t n = shm_pop_buf(buf, sizeof(buf));  // 批量读
```

### 读取 DCMI/DMA_Catch 数据

```c
// CM7 侧直接读固定地址
SCB_InvalidateDCache_by_Addr((uint32_t*)0x30002000, 3840 * 2);
// adc_a = DCMI_ADC_A[0..3839]
// adc_b = DCMI_ADC_B[0..3839]
```

## 六、关键 API

| 函数 | 所属文件 | 说明 |
|------|---------|------|
| `shm_push(byte)` | shared_buf.h | M4 写一字节到 SHM_RING |
| `shm_push_buf(data, len)` | shared_buf.h | M4 写连续字节 |
| `shm_pop(&byte)` | shared_buf.h | CM7 读一字节 |
| `shm_pop_buf(buf, len)` | shared_buf.h | CM7 批量读 |
| `shm_config_notify()` | CM7 main.c | CM7 写完配置后通知 M4 |
| `apply_xxx_config_from_shm()` | CM4 freertos.c | M4 读 SHM_CONFIG 重配外设 |
| `SHM_TX_BUF` / `SHM_TX_LEN` | shared_config.h | M7→M4 发送数据通道 |

## 七、注意事项

1. **D2 SRAM1 地址别名**：`0x10000000` 和 `0x30000000` 是同一块物理内存。CM4 链接脚本已预留头 64KB 给共享内存
2. **CM7 侧写共享内存后**必须 `__DMB()` + `__DSB()`，否则 CM4 可能看不到最新数据
3. **CM7 侧读共享内存前**必须 `SCB_InvalidateDCache_by_Addr()`
4. **CM4 没有 D-Cache**，读写直通物理内存，不需要维护缓存
5. **协议切换安全**：Proto_Select 杀旧协议 Task 前会调 apply 函数，其中的 HAL_DeInit 清理外设状态
