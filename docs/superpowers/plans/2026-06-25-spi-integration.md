# SPI6 协议集成 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把队友 `my_spi_check.c/h` 移植到 CM4，作为第 4 个协议（slave+master 双角色）接入双核架构，CM7 Settings 屏加 Role 栏。

**Architecture:** CM4 裸机 main 循环按 `active_proto==2` 分支：slave 模式由 CS(PG10)EXTI + BDMA 预 arm 自动收 → `spi_range_buffer`；master 模式周期 `My_SPI_SendReceive` → TxRxCpltCallback 推入同一 ring。main 循环统一 `SPI_RangeBuffer_Read → shm_push → CM7 波形`。SPI DMA buffer 放 RAM_D3(0x38000000,BDMA 只访问 D3)。

**Tech Stack:** STM32H747 双核（CM4 裸机 + CM7 FreeRTOS/TouchGFX），HAL，CMake，GCC ARM。**无单元测试框架** —— 验证 = 编译通过 + 烧录 + 串口/屏幕观察（每个 task 的 gate 是"CM4/CM7 编译 0 error"，行为验证集中在 Task 11）。

**Spec:** [docs/superpowers/specs/2026-06-25-spi-integration-design.md](../specs/2026-06-25-spi-integration-design.md)

**关键约束（贯穿所有 task）：**
- BDMA 只能访问 D3 域（SRAM4@0x38000000），`rx_spi_buffer`/`tx_spi_buffer` 必须在 `.RAM_D3` 段
- CM4 无 DCache，SRAM4 访问不需 MPU 配置
- master 模式 `My_SPI_SendReceive` 是 IT 非阻塞，tx/rx buffer 必须 file-scope static

---

## File Structure

| 文件 | 责任 | 类型 |
|---|---|---|
| `CM4/Core/Inc/shared_config.h` | spi_config_t 加 role/cs_polarity | 改 |
| `CM4/stm32h747xx_flash_CM4.ld` | RAM_D3 区 + .RAM_D3 段 | 改 |
| `CM4/Core/Inc/my_spi_check.h` | SPI 协议分析接口（队友移植）| 新 |
| `CM4/Core/Src/my_spi_check.c` | SPI 协议分析实现（队友移植 + 6 处裁剪）| 新 |
| `CM4/Core/Src/main.c` | SPI init + apply + 主循环 + regen 兜底 | 改 |
| `CM4/Core/Src/gpio.c` | CS 初值 RESET→SET | 改 |
| `CM4/CMakeLists.txt` | +my_spi_check.c | 改 |
| `CM7/.../Settings_ScreenView.hpp` | +sRole 成员 + snap | 改 |
| `CM7/.../Settings_ScreenView.cpp` | s_data 砍到 ≤8 + sRole 全接线 | 改 |

---

## Task 1: 准备队友源码 + 核对硬件配置

**Files:** 无改动（只读 + clone）

- [ ] **Step 1: clone 队友仓库到临时目录**（防止 /tmp 被清）

```bash
rm -rf /tmp/pa_clone && git clone --depth 1 https://github.com/desk-win/Protocol_Analysis.git /tmp/pa_clone
ls /tmp/pa_clone/CM4/Core/Src/my_spi_check.c /tmp/pa_clone/CM4/Core/Inc/my_spi_check.h
```
Expected: 两个文件路径都列出（clone OK）。Windows 实际路径 `C:/Users/admin/AppData/Local/Temp/pa_clone/`。

- [ ] **Step 2: 核对 SPI6 硬件配置（只读，确认 spec §2 无误）**

读 [CM4/Core/Src/spi.c:120-151](../../../CM4/Core/Src/spi.c#L120-L151) 确认：
- `hdma_spi6_tx.Instance = BDMA_Channel0`，`hdma_spi6_rx.Instance = BDMA_Channel1`
- `PeriphDataAlignment = DMA_PDATAALIGN_BYTE`，`MemDataAlignment = DMA_MDATAALIGN_BYTE`（**byte 对齐 → datasize ≤8**，对应 Task 9 砍 s_data）

读 [CM4/Core/Src/bdma.c:46-51](../../../CM4/Core/Src/bdma.c#L46-L51) 确认 BDMA_Channel0/1_IRQn 已开（prio 5）。

若任一项不符 → 停下，先核对 .ioc / spec，不要继续。

- [ ] **Step 3: 不提交**（本 task 无代码改动）

---

## Task 2: shared_config.h 加 role + cs_polarity 字段

**Files:**
- Modify: `CM4/Core/Inc/shared_config.h:27-32`（spi_config_t）+ `:65-68`（默认宏）
- 同步：`CM7/Core/Inc/shared_config.h`（两个核各有一份，**必须改一致**）

> ⚠️ shared_config.h 在 CM4 和 CM7 各有一份副本，两份都要改，否则 sizeof 不一致致 DCache 维护错位。

- [ ] **Step 1: CM4 shared_config.h —— spi_config_t 加 2 字段**

把 [shared_config.h:27-32](../../../CM4/Core/Inc/shared_config.h#L27-L32)：
```c
typedef struct {
    uint8_t  mode;         /* 0-3: CPOL/CPHA 组合（0=00, 1=01, 2=10, 3=11）*/
    uint8_t  datasize;     /* 4-16 bit */
    uint32_t baudrate;     /* 实际波特率（CM4 算 prescaler）*/
    uint8_t  firstbit;     /* 0=MSB first, 1=LSB first */
} spi_config_t;
```
改成：
```c
typedef struct {
    uint8_t  role;         /* 0=Slave(默认), 1=Master */
    uint8_t  cs_polarity;  /* 0=Active-Low(默认), 1=Active-High（v1 不进 UI，硬编码 0）*/
    uint8_t  mode;         /* 0-3: CPOL/CPHA 组合（0=00, 1=01, 2=10, 3=11）*/
    uint8_t  datasize;     /* 4-8 bit（v1 限制：BDMA byte 对齐）*/
    uint32_t baudrate;     /* slave 无效；master 见 spi_prescaler_from_baud */
    uint8_t  firstbit;     /* 0=MSB first, 1=LSB first */
} spi_config_t;
```

- [ ] **Step 2: CM4 shared_config.h —— 加默认宏**

在 [shared_config.h:68](../../../CM4/Core/Inc/shared_config.h#L68) `#define SPI_DEFAULT_FIRSTBIT 0U` 后加：
```c
#define SPI_DEFAULT_ROLE        0U
#define SPI_DEFAULT_CS_POL      0U
```

- [ ] **Step 3: CM7 shared_config.h —— 同样改动**

读 `CM7/Core/Inc/shared_config.h`，对其 spi_config_t 和默认宏做**完全一样**的改动（Step 1+2 的内容）。两核结构体必须字节对齐一致。

- [ ] **Step 4: 提交**

```bash
git add CM4/Core/Inc/shared_config.h CM7/Core/Inc/shared_config.h
git commit -m "SPI: shared_config.h spi_config_t +role +cs_polarity"
```

---

## Task 3: linker 加 RAM_D3 区 + .RAM_D3 段

**Files:**
- Modify: `CM4/stm32h747xx_flash_CM4.ld:39-44`（MEMORY）+ `:59-64` 后（SECTIONS）

- [ ] **Step 1: MEMORY 块加 RAM_D3**

把 [stm32h747xx_flash_CM4.ld:39-44](../../../CM4/stm32h747xx_flash_CM4.ld#L39-L44)：
```ld
MEMORY
{
FLASH (rx)      : ORIGIN = 0x08100000, LENGTH = 1024K
RAM (xrw)      : ORIGIN = 0x10000000, LENGTH = 288K
OPEN_AMP_SHMEM (xrw)  : ORIGIN = 0x38000400, LENGTH = 63K
}
```
改成（加一行 RAM_D3，用 0x38000000 起的 1K 空隙，**不动 OPEN_AMP_SHMEM**）：
```ld
MEMORY
{
FLASH (rx)      : ORIGIN = 0x08100000, LENGTH = 1024K
RAM (xrw)      : ORIGIN = 0x10000000, LENGTH = 288K
RAM_D3 (xrw)   : ORIGIN = 0x38000000, LENGTH = 1K
OPEN_AMP_SHMEM (xrw)  : ORIGIN = 0x38000400, LENGTH = 63K
}
```

- [ ] **Step 2: SECTIONS 加 .RAM_D3 段**

在 [stm32h747xx_flash_CM4.ld:64](../../../CM4/stm32h747xx_flash_CM4.ld#L64) `.resource_table` 段的 `} >OPEN_AMP_SHMEM` 之后、`.isr_vector` 之前插入：
```ld
  /* SPI6 BDMA buffer：BDMA 只能访问 D3 域 SRAM4，rx_spi_buffer/tx_spi_buffer 用 __attribute__((section(".RAM_D3"))) */
  .RAM_D3 (NOLOAD) :
  {
    . = ALIGN(4);
    *(.RAM_D3*)
    . = ALIGN(4);
  } >RAM_D3
```

- [ ] **Step 3: 提交**

```bash
git add CM4/stm32h747xx_flash_CM4.ld
git commit -m "SPI: linker RAM_D3 段 (BDMA buffer @ 0x38000000)"
```

---

## Task 4: 移植 my_spi_check.h

**Files:**
- Create: `CM4/Core/Inc/my_spi_check.h`

- [ ] **Step 1: 从队友拷贝**

```bash
cp /tmp/pa_clone/CM4/Core/Inc/my_spi_check.h d:/CubeMx/CubeMx_project/qiansai/CM4/Core/Inc/my_spi_check.h
```

- [ ] **Step 2: 删 RTOS include**

把拷过来的 [my_spi_check.h:9-11](../../../CM4/Core/Inc/my_spi_check.h#L9-L11) 三行：
```c
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
```
**整段删除**（CM4 裸机不用 RTOS）。保留 `stm32h7xx_hal.h`/`spi.h`/`math.h`/`stdint.h`/`string.h`。

- [ ] **Step 3: 改 SPI_RangeBuffer_Read 返回类型 + 补声明**

把：
```c
uint8_t SPI_RangeBuffer_Read(uint8_t *data, uint32_t data_len);
```
改成：
```c
uint32_t SPI_RangeBuffer_Read(uint8_t *data, uint32_t data_len);
```

在 `extern My_SPI_Deploy spi_deploy;` 后加：
```c
extern uint8_t if_busy;   /* main 循环 master 分支查 busy（IT 非阻塞）*/
```

在文件末尾 `#endif` 前的函数声明区加 2 个新函数声明：
```c
uint32_t spi_datasize_from_u8(uint8_t bits);       /* UI datasize(4-8) → HAL SPI_DATASIZE_NBIT */
uint16_t spi_prescaler_from_baud(uint32_t baud);   /* UI baud 档位 → prescaler（master 用）*/
```

- [ ] **Step 4: 不单独提交**（和 Task 5 的 .c 一起提交）

---

## Task 5: 移植 my_spi_check.c（6 处裁剪）

**Files:**
- Create: `CM4/Core/Src/my_spi_check.c`

- [ ] **Step 1: 从队友拷贝**

```bash
cp /tmp/pa_clone/CM4/Core/Src/my_spi_check.c d:/CubeMx/CubeMx_project/qiansai/CM4/Core/Src/my_spi_check.c
```

- [ ] **Step 2: 裁剪 ① 删多余 include**

删掉这两行（[my_spi_check.c:2](../../../CM4/Core/Src/my_spi_check.c#L2) 和 :13 附近）：
```c
#include "cmsis_os2.h"
#include "my_dma_catch.h"
```

- [ ] **Step 3: 裁剪 ② 删 RTOS task 段**

删掉文件末尾整个 RTOS 段（队友原 :469-485）：
```c
/*********************************这个部分为接入RTOS***********************************/
/**
*   @brief 这个函数放在MX_FREERTOS_Init里面，用来创建所有的spi任务
*
****/

void My_SPI_Init_Task(void *argument){
    My_SPI_Init();
    while(1){

    }
}

void My_SPI_Task(void){
    //创建初始化任务
    xTaskCreate(My_SPI_Init_Task, "My_SPI_Init_Task", 128, NULL, osPriorityNormal, NULL);
}
```

- [ ] **Step 4: 裁剪 ③ 修 analyse_index 越界**

在 `HAL_GPIO_EXTI_Callback` 的 CS 下降沿分支，把：
```c
            if (HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_10) == GPIO_PIN_RESET) {
                analyse_index++;
                spi_analyse[analyse_index].cs_low_tick = Get_Sys_us();         //记录cs引脚拉低的时刻
```
改成（加一行越界保护，spi_analyse 数组大小 20）：
```c
            if (HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_10) == GPIO_PIN_RESET) {
                analyse_index++;
                if (analyse_index >= 20) analyse_index = 1;   /* 越界保护：spi_analyse[20]，index 0 存累计 */
                spi_analyse[analyse_index].cs_low_tick = Get_Sys_us();         //记录cs引脚拉低的时刻
```

- [ ] **Step 5: 裁剪 ④ SPI_RangeBuffer_Read 改返回字节数**

把整个函数（队友原 :120-135）：
```c
uint8_t SPI_RangeBuffer_Read(uint8_t *data, uint32_t data_len){
    uint8_t if_empty = 1;
    uint32_t data_index = 0;                //索引
    if (spi_range_buffer.buffer_tail != spi_range_buffer.buffer_head) {
        //当缓冲区里面还有数据的时候
        while (spi_range_buffer.buffer_tail != spi_range_buffer.buffer_head) {
            data[data_index] = spi_range_buffer.range_buffer[spi_range_buffer.buffer_tail];
            data_index++;
            if(data_index >= data_len) break;
            spi_range_buffer.buffer_tail = (spi_range_buffer.buffer_tail + 1)%SPI_RANGE_BUFFER_LEN;
        }
    }else {
        if_empty = 0;
    }
    return if_empty;
}
```
改成：
```c
uint32_t SPI_RangeBuffer_Read(uint8_t *data, uint32_t data_len){
    uint32_t data_index = 0;
    while (spi_range_buffer.buffer_tail != spi_range_buffer.buffer_head) {
        data[data_index] = spi_range_buffer.range_buffer[spi_range_buffer.buffer_tail];
        spi_range_buffer.buffer_tail = (spi_range_buffer.buffer_tail + 1) % SPI_RANGE_BUFFER_LEN;
        data_index++;
        if (data_index >= data_len) break;
    }
    return data_index;   /* 实际读取字节数（0=空）*/
}
```

- [ ] **Step 6: 裁剪 ⑤ master 回调推入 ring + 全局指针**

在 `uint8_t if_busy = 0;`（队友原 :77）后加 2 个全局：
```c
uint8_t if_busy = 0;                            //作为主机发送数据的时候检查是否发送完成标志位
static uint8_t *g_spi_rx_ptr = NULL;            /* master: My_SPI_SendReceive 记录 rx 指针，TxRxCpltCallback 推入 ring */
static uint16_t g_spi_rx_len = 0;
```

在 `My_SPI_SendReceive`（队友原 :353）入口记 rx 指针，把：
```c
void My_SPI_SendReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t len){
    if (spi_deploy.spi_role == MY_SPI_MASTER) {
        if (if_busy == 0) {
            if_busy = 1;
            if (spi_deploy.cs_polarity == 0) CS_Pin_State(0);
            else CS_Pin_State(1);
            HAL_SPI_TransmitReceive_IT(&hspi6, tx_data, rx_data, len);
        }
    }
}
```
改成：
```c
void My_SPI_SendReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t len){
    if (spi_deploy.spi_role == MY_SPI_MASTER) {
        if (if_busy == 0) {
            if_busy = 1;
            g_spi_rx_ptr = rx_data;            /* 记录，供 TxRxCpltCallback 推入 ring */
            g_spi_rx_len = len;
            if (spi_deploy.cs_polarity == 0) CS_Pin_State(0);
            else CS_Pin_State(1);
            HAL_SPI_TransmitReceive_IT(&hspi6, tx_data, rx_data, len);
        }
    }
}
```

在 `HAL_SPI_TxRxCpltCallback` master 分支（队友原 :367-370）加 ring 推入，把：
```c
        if (spi_deploy.spi_role == MY_SPI_MASTER) {
            if (spi_deploy.cs_polarity == 0) CS_Pin_State(1);
            else CS_Pin_State(0);
            if_busy = 0;
        }else if(spi_deploy.spi_role == MY_SPI_SLAVE){
```
改成：
```c
        if (spi_deploy.spi_role == MY_SPI_MASTER) {
            if (spi_deploy.cs_polarity == 0) CS_Pin_State(1);
            else CS_Pin_State(0);
            /* 把刚收到的 rx 字节推入 ring（统一 slave/master 数据出口 → main 循环读）*/
            if (g_spi_rx_ptr) {
                for (uint16_t i = 0; i < g_spi_rx_len; i++) SPI_RangeBuffer_Wirte(g_spi_rx_ptr[i]);
                g_spi_rx_ptr = NULL;
            }
            if_busy = 0;
        }else if(spi_deploy.spi_role == MY_SPI_SLAVE){
```

- [ ] **Step 7: 裁剪 ⑥ 加 datasize / prescaler 映射函数**

在 `SPI_BaudRatePrescaler` 函数（队友原 :194-206）后加 2 个函数：
```c
/**
*   @brief UI datasize(4-8) → HAL SPI_DATASIZE_NBIT 枚举（v1 ≤8，BDMA byte 对齐限制）
*   @param bits: 4/5/6/7/8
*/
uint32_t spi_datasize_from_u8(uint8_t bits)
{
    switch (bits) {
        case 4: return SPI_DATASIZE_4BIT;
        case 5: return SPI_DATASIZE_5BIT;
        case 6: return SPI_DATASIZE_6BIT;
        case 7: return SPI_DATASIZE_7BIT;
        case 8: default: return SPI_DATASIZE_8BIT;
    }
}

/**
*   @brief UI baudrate 档位 → prescaler（slave 不用；master 用）
*   ⚠️ CM7 baud 标签按 12MHz 算，.ioc 标 SPI6Freq=120MHz，实际波特率可能差 10×（spec §8 待验证）
*/
uint16_t spi_prescaler_from_baud(uint32_t baud)
{
    switch (baud) {
        case 187500:  return 64;
        case 375000:  return 32;
        case 750000:  return 16;
        case 1500000: return 8;
        case 3000000: return 4;
        case 6000000: return 2;
        default:      return 64;
    }
}
```

- [ ] **Step 8: 提交（.h + .c 一起）**

```bash
git add CM4/Core/Inc/my_spi_check.h CM4/Core/Src/my_spi_check.c
git commit -m "SPI: 移植 my_spi_check.c/h (剥RTOS+my_dma_catch, 修analyse越界, Read返字节数, master推ring)"
```

---

## Task 6: main.c 接线（init + apply + 主循环 + regen 兜底）

**Files:**
- Modify: `CM4/Core/Src/main.c`（多个 USER CODE 区）

- [ ] **Step 1: include + PV 静态变量**

[main.c:34](../../../CM4/Core/Src/main.c#L34) `#include "my_i2c_check.h"` 后加：
```c
#include "my_spi_check.h"
```

[main.c:70](../../../CM4/Core/Src/main.c#L70) `volatile uint8_t g_config_pending = 0;` 后加（USER CODE PV 区）：
```c
/* SPI master 模式 IT 非阻塞：tx/rx buffer 必须 file-scope static（不能用栈）*/
static uint8_t spi_tx_buf[8];
static uint8_t spi_rx_tmp[8];
```

- [ ] **Step 2: PFP 原型**

[main.c:79](../../../CM4/Core/Src/main.c#L79) `static void apply_i2c_config_from_shm(void);` 后加：
```c
static void apply_spi_config_from_shm(void);    /* 读 SHM_CONFIG->spi → My_SPI_Init 重配 SPI6（slave/master）*/
```

- [ ] **Step 3: USER CODE 2 —— SPI init + CS regen 兜底**

[main.c:153](../../../CM4/Core/Src/main.c#L153) `My_I2C_Init(...)` 后加：
```c

  /* SPI 初始化：默认从机模式（mode0/CS低有效），CS 当 EXTI 双边沿 + DMA 预 arm 收数据。
   * HAL_GPIO_EXTI_Callback（在 my_spi_check.c）自动收帧入 spi_range_buffer。*/
  HAL_GPIO_WritePin(SPI6_CS_GPIO_Port, SPI6_CS_Pin, GPIO_PIN_SET);  /* CS regen 兜底：gpio.c:49 regen 重置成 RESET */
  My_SPI_Init(0, MY_SPI_SLAVE, 0, 64, 0);
```

- [ ] **Step 4: g_config_pending 分支加 SPI**

把 [main.c:175-177](../../../CM4/Core/Src/main.c#L175-L177)：
```c
      if (SHM_CONFIG->active_proto == 1) apply_uart_config_from_shm();
      else if (SHM_CONFIG->active_proto == 3) apply_i2c_config_from_shm();
      else if (SHM_CONFIG->active_proto == 4) apply_can_config_from_shm();
```
改成：
```c
      if (SHM_CONFIG->active_proto == 1) apply_uart_config_from_shm();
      else if (SHM_CONFIG->active_proto == 2) apply_spi_config_from_shm();
      else if (SHM_CONFIG->active_proto == 3) apply_i2c_config_from_shm();
      else if (SHM_CONFIG->active_proto == 4) apply_can_config_from_shm();
```

- [ ] **Step 5: 主循环加 SPI 数据通路**

把 [main.c:188-206](../../../CM4/Core/Src/main.c#L188-L206) 的 I2C 分支：
```c
    /* I2C 模式：从机收到的帧（my_i2c_data.i2c_slave_rxdata）→ shm 给 CM7 */
    else if (SHM_CONFIG->active_proto == 3) {
      if (my_i2c_data.task_done) {
        my_i2c_data.task_done = 0;
        for (uint32_t i = 0; i < my_i2c_data.i2c_slave_rxlen; i++)
          shm_push(my_i2c_data.i2c_slave_rxdata[i]);
      }
    }
```
后面（`else {` UART 自环回分支**之前**）插入 SPI 分支：
```c
    /* SPI 模式：master 周期主动收发 → ring；slave 由 CS EXTI 自动填 ring → shm 给 CM7 */
    else if (SHM_CONFIG->active_proto == 2) {
      if (spi_deploy.spi_role == MY_SPI_MASTER) {
        /* master 自环回/流量发生器：v1 TX 写死递增（spec §8 #3，hex 输入留 v2）*/
        if (bm_tick % 100 == 0 && if_busy == 0) {
          for (int i = 0; i < 5; i++) spi_tx_buf[i] = (uint8_t)(tx_base + i);
          My_SPI_SendReceive(spi_tx_buf, spi_rx_tmp, 5);
          tx_base += 5;
        }
      }
      uint32_t n = SPI_RangeBuffer_Read(rx_buf, sizeof(rx_buf));
      for (uint32_t i = 0; i < n; i++) shm_push(rx_buf[i]);
    }
```
> `rx_buf`/`tx_base` 复用 [main.c:166-168](../../../CM4/Core/Src/main.c#L166-L168) 已有声明的栈变量（阻塞读 ring，读完即弃，OK）。`spi_tx_buf`/`spi_rx_tmp` 是 Step 1 加的 static（IT 非阻塞需持久）。

- [ ] **Step 6: USER CODE 4 —— apply_spi_config_from_shm 实现**

[main.c:322](../../../CM4/Core/Src/main.c#L322) `apply_i2c_config_from_shm` 函数结束后（USER CODE 4 区）加：
```c

/* 读 SHM_CONFIG->spi → My_SPI_Init 重配 SPI6。
 * role: 0=Slave（被动收，baudrate 无效），1=Master（主动收发）。
 * datasize 在 init 前赋 hspi6.Init.DataSize（队友 My_SPI_Init 不覆盖该字段，HAL_SPI_DeInit 也不清 Init 结构体）。
 * cs_polarity v1 硬编码 0（低有效），UI 未暴露。*/
static void apply_spi_config_from_shm(void)
{
  spi_config_t *s = &SHM_CONFIG->spi;
  hspi6.Init.DataSize = spi_datasize_from_u8(s->datasize);
  hspi6.Init.FirstBit = (s->firstbit) ? SPI_FIRSTBIT_LSB : SPI_FIRSTBIT_MSB;
  if (s->role == 1) {
    My_SPI_Init(s->mode, MY_SPI_MASTER, 0, spi_prescaler_from_baud(s->baudrate), 0);
  } else {
    My_SPI_Init(s->mode, MY_SPI_SLAVE, 0, 64, 0);
  }
  uart1_printf("[CM4] SPI reconfig OK: role=%s mode=%u ds=%u first=%u\r\n",
               s->role ? "MASTER" : "SLAVE", (unsigned)s->mode,
               (unsigned)s->datasize, (unsigned)s->firstbit);
}
```

- [ ] **Step 7: 提交**

```bash
git add CM4/Core/Src/main.c
git commit -m "SPI: main.c 接线 (init + apply_spi + 主循环双角色通路 + CS regen兜底)"
```

---

## Task 7: gpio.c CS 初值 + CMakeLists

**Files:**
- Modify: `CM4/Core/Src/gpio.c:49`
- Modify: `CM4/CMakeLists.txt:60-66`

- [ ] **Step 1: gpio.c CS 初值 RESET→SET**

把 [gpio.c:49](../../../CM4/Core/Src/gpio.c#L49)：
```c
  HAL_GPIO_WritePin(SPI6_CS_GPIO_Port, SPI6_CS_Pin, GPIO_PIN_RESET);
```
改成：
```c
  HAL_GPIO_WritePin(SPI6_CS_GPIO_Port, SPI6_CS_Pin, GPIO_PIN_SET);
```
> ⚠️ 该行不在 USER CODE 区，**CubeMX regen 会重置回 RESET**。真正生效靠 Task 6 Step 3 的 main.c USER CODE 2 兜底。改这里只是为了让上电到 My_SPI_Init 之间 CS 也是高。

- [ ] **Step 2: CMakeLists 加 my_spi_check.c**

把 [CMakeLists.txt:60-66](../../../CM4/CMakeLists.txt#L60-L66)：
```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/my_uart_check.c
    Core/Src/my_can_check.c
    Core/Src/my_i2c_check.c
    Core/Src/my_dwt_count.c
)
```
改成：
```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/my_uart_check.c
    Core/Src/my_can_check.c
    Core/Src/my_i2c_check.c
    Core/Src/my_spi_check.c
    Core/Src/my_dwt_count.c
)
```

- [ ] **Step 3: 提交**

```bash
git add CM4/Core/Src/gpio.c CM4/CMakeLists.txt
git commit -m "SPI: gpio CS 空闲高 + CMakeLists 加 my_spi_check.c"
```

---

## Task 8: CM7 Settings_ScreenView.hpp 加 sRole

**Files:**
- Modify: `CM7/TouchGFX/gui/include/gui/settings_screen_screen/Settings_ScreenView.hpp:41,57`

- [ ] **Step 1: 加 sRole 成员变量**

把 [Settings_ScreenView.hpp:41](../../../CM7/TouchGFX/gui/include/gui/settings_screen_screen/Settings_ScreenView.hpp#L41)：
```cpp
    /* SPI 参数 idx */
    uint8_t sMode, sData, sBaud, sFirst;
```
改成：
```cpp
    /* SPI 参数 idx */
    uint8_t sMode, sData, sBaud, sFirst, sRole;   /* sRole: 0=Slave, 1=Master */
```

- [ ] **Step 2: snap 结构体加 sRole**

把 [Settings_ScreenView.hpp:57](../../../CM7/TouchGFX/gui/include/gui/settings_screen_screen/Settings_ScreenView.hpp#L57)：
```cpp
    struct { uint8_t protoIdx, uBaud, uData, uStop, uPar, uFlow, sMode, sData, sBaud, sFirst, iClock, iAddr, cBaud, cMode; } snap;
```
改成：
```cpp
    struct { uint8_t protoIdx, uBaud, uData, uStop, uPar, uFlow, sMode, sData, sBaud, sFirst, sRole, iClock, iAddr, cBaud, cMode; } snap;
```

- [ ] **Step 3: 不单独提交**（和 Task 9 的 .cpp 一起）

---

## Task 9: CM7 Settings_ScreenView.cpp（s_data 砍 + sRole 全接线）

**Files:**
- Modify: `CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp`（多处）

- [ ] **Step 1: s_data 砍到 ≤8 + 加 s_roleD 标签**

把 [Settings_ScreenView.cpp:22](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L22)：
```cpp
static const uint8_t  s_data[7]  = {4, 5, 6, 7, 8, 12, 16};
```
改成：
```cpp
static const uint8_t  s_data[5]  = {4, 5, 6, 7, 8};        /* v1 ≤8：BDMA byte 对齐限制（spec §8 #1）*/
static const char*    s_roleD[2] = {"Slave", "Master"};
```

- [ ] **Step 2: 构造函数初始化 sRole**

[Settings_ScreenView.cpp:44](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L44) 把：
```cpp
      sMode(0), sData(4), sBaud(3), sFirst(0),
```
改成：
```cpp
      sMode(0), sData(4), sBaud(3), sFirst(0), sRole(0),
```

- [ ] **Step 3: setupScreen 读 sRole + s_data 映射循环上限改 5**

[Settings_ScreenView.cpp:76-77](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L76-L77) 把：
```cpp
    { uint8_t sd = SHM_CONFIG->spi.datasize;
      sData = 4; for (uint8_t i = 0; i < 7; i++) if (s_data[i] == sd) { sData = i; break; } }
```
改成：
```cpp
    { uint8_t sd = SHM_CONFIG->spi.datasize;
      sData = 4; for (uint8_t i = 0; i < 5; i++) if (s_data[i] == sd) { sData = i; break; } }
    sRole = (SHM_CONFIG->spi.role < 2) ? SHM_CONFIG->spi.role : 0;
```

- [ ] **Step 4: setupScreen snap 存 sRole**

[Settings_ScreenView.cpp:92](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L92) 把：
```cpp
    snap.sMode = sMode; snap.sData = sData; snap.sBaud = sBaud; snap.sFirst = sFirst;
```
改成：
```cpp
    snap.sMode = sMode; snap.sData = sData; snap.sBaud = sBaud; snap.sFirst = sFirst; snap.sRole = sRole;
```

- [ ] **Step 5: refreshAll case 1 row5 显示 Role**

[Settings_ScreenView.cpp:213-219](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L213-L219) 把：
```cpp
        case 1: /* SPI 4 参数 + row5 空 */
            ROW(1, "%s Mode: %s",    s_modeD[sMode]);
            ROW(2, "%s Data: %u",    (unsigned)s_data[sData]);
            ROW(3, "%s Baud: %lu",   (unsigned long)s_baud[sBaud]);
            ROW(4, "%s First: %s",   s_first[sFirst]);
            ROWEMPTY(5);
            break;
```
改成：
```cpp
        case 1: /* SPI 5 参数：mode/data/baud/first/role */
            ROW(1, "%s Mode: %s",    s_modeD[sMode]);
            ROW(2, "%s Data: %u",    (unsigned)s_data[sData]);
            ROW(3, "%s Baud: %lu",   (unsigned long)s_baud[sBaud]);
            ROW(4, "%s First: %s",   s_first[sFirst]);
            ROW(5, "%s Role: %s",    s_roleD[sRole]);
            break;
```

- [ ] **Step 6: applyConfig 写 spi.role + snap**

[Settings_ScreenView.cpp:247](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L247) `SHM_CONFIG->spi.firstbit = sFirst;` 后加一行：
```cpp
    SHM_CONFIG->spi.role           = sRole;
```

[Settings_ScreenView.cpp:260](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L260) 把：
```cpp
    snap.sMode = sMode; snap.sData = sData; snap.sBaud = sBaud; snap.sFirst = sFirst;
```
改成：
```cpp
    snap.sMode = sMode; snap.sData = sData; snap.sBaud = sBaud; snap.sFirst = sFirst; snap.sRole = sRole;
```

- [ ] **Step 7: onPlusClick 加 sRole 档位 + sData 模改 5**

[Settings_ScreenView.cpp:301-303](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L301-L303) 把：
```cpp
        case 1*16+2: sData  = (sData  + 1) % 7; break;
        case 1*16+3: sBaud  = (sBaud  + 1) % 6; break;
        case 1*16+4: sFirst = (sFirst + 1) % 2; break;
```
改成：
```cpp
        case 1*16+2: sData  = (sData  + 1) % 5; break;
        case 1*16+3: sBaud  = (sBaud  + 1) % 6; break;
        case 1*16+4: sFirst = (sFirst + 1) % 2; break;
        case 1*16+5: sRole  = (sRole  + 1) % 2; break;
```

- [ ] **Step 8: onMinusClick 加 sRole 档位 + sData 回绕改 4**

[Settings_ScreenView.cpp:325-327](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L325-L327) 把：
```cpp
        case 1*16+2: sData  = (sData  == 0) ? 6 : (sData  - 1); break;
        case 1*16+3: sBaud  = (sBaud  == 0) ? 5 : (sBaud  - 1); break;
        case 1*16+4: sFirst = (sFirst == 0) ? 1 : (sFirst - 1); break;
```
改成：
```cpp
        case 1*16+2: sData  = (sData  == 0) ? 4 : (sData  - 1); break;
        case 1*16+3: sBaud  = (sBaud  == 0) ? 5 : (sBaud  - 1); break;
        case 1*16+4: sFirst = (sFirst == 0) ? 1 : (sFirst - 1); break;
        case 1*16+5: sRole  = (sRole  == 0) ? 1 : (sRole  - 1); break;
```

- [ ] **Step 9: hasChanges 加 sRole 对比**

读 [Settings_ScreenView.cpp:266-280](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L266-L280) `hasChanges()` 的 return 语句，在 `sFirst != snap.sFirst` 后加 `|| sRole != snap.sRole`。
（具体行：return 那一行含 `sFirst != snap.sFirst ||` → 改成 `sFirst != snap.sFirst || sRole != snap.sRole ||`）

- [ ] **Step 10: 提交（.hpp + .cpp 一起）**

```bash
git add CM7/TouchGFX/gui/include/gui/settings_screen_screen/Settings_ScreenView.hpp \
        CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp
git commit -m "SPI: CM7 Settings 加 Role 栏 + s_data 砍到 ≤8 (BDMA byte 对齐)"
```

---

## Task 10: 编译 CM4 + CM7

**Files:** 无改动

- [ ] **Step 1: 编译 CM4**

用你惯用方式构建 CM4（CubeIDE 或 `cmake --build`）。预期：**0 error**。

常见报错排查：
- `section .RAM_D3 not found` → linker 没改对（Task 3）
- `undefined reference to My_SPI_Init` → CMakeLists 没加（Task 7）或 .h 未 include（Task 6 Step 1）
- `SPI_RangeBuffer_Read type mismatch` → .h 和 .c 返回类型不一致（Task 4 Step 3 vs Task 5 Step 5）
- `if_busy undeclared` → .h 没加 extern（Task 4 Step 3）

- [ ] **Step 2: 编译 CM7**

构建 CM7。预期：**0 error**。

常见报错排查：
- `'s_roleD' was not declared` → Step 1 没加
- `no member named 'sRole'` → hpp 没改（Task 8）
- `s_data[6] out of range` → sData 默认值或映射循环没改到 %5（Task 9 Step 3/7/8）

- [ ] **Step 3: 查 CM4 .map 确认 RAM_D3 段落位**

构建产物里找 CM4 的 `.map` 文件，grep：
```bash
grep -i "RAM_D3\|rx_spi_buffer\|tx_spi_buffer" <cm4_build_dir>/*.map
```
Expected: `rx_spi_buffer` 和 `tx_spi_buffer` 地址在 `0x38000000-0x38000400` 区间。若落在 `0x10000000`(DTCM) → linker 段没生效，BDMA 会读错，**必须修**。

- [ ] **Step 4: 不提交**（无源码改动）

---

## Task 11: 烧录 + 运行时验证

**Files:** 无改动（按 spec §7）

- [ ] **Step 1: 烧录双核固件**（CM7 + CM4）

- [ ] **Step 2: 上电看 CM4 UART1 打印**

打开 UART1 串口（PG14/PG9 那个，队友 printf 通道），预期看到：
```
[CM4] enter USER CODE 2 (boot sync 已过)
[CM4] shm_init done: head/tail=0
[CM4] bare-metal: UART6 grab -> shm (PG14<->PG9 self-loop)
```
（开机默认 active_proto=0/UART，所以 SPI 还没初始化也没打印 —— 这是正常的，SPI init 在 USER CODE 2 但 reconfig 打印只在 Apply 后）

- [ ] **Step 3: CM7 Settings 屏切到 SPI，选 Slave，Apply**

触摸 Settings → Protocol 行 +/- 切到 SPI → row5 Role 确认是 Slave → Apply。

预期 UART1 打印：
```
[CM4] SPI reconfig OK: role=SLAVE mode=0 ds=8 first=0
```

- [ ] **Step 4: SPI Slave 收数据验证**

外部 SPI master（另一块板子/逻辑分析仪/USB-SPI 适配器）按 mode0/8bit/MSB 驱动 CS(PG10)↓ + SCK(PG13) + MOSI(PA7) 发几个字节。

预期：CM7 data_screen 波形/数据更新（收到字节）。若没数据：
- 确认外部 master 的 CPOL/CPHA 和 mode 一致
- 确认 PG10 CS 被 master 拉低（用万用表/示波器）
- UART1 看 `spi_deploy.spi_error.error_code` 有没有报错（可选加 printf）

- [ ] **Step 5: SPI Master 自环回验证**

Settings → SPI → row5 Role 切 Master → Apply。**MOSI(PA7)↔MISO(PG12) 用跳线短接**。

预期 UART1 打印 `role=MASTER`，CM7 data_screen 看到递增字节波形（0,1,2,3,4 / 5,6,7,8,9 ...）。

- [ ] **Step 6: datasize UI 验证**

Settings → SPI → row2 Data +/- 翻：只能看到 4/5/6/7/8（**不应出现 12/16**）。

- [ ] **Step 7: 切协议回归**

Settings 切 SPI → UART/I2C/CAN 各试一次 Apply，确认其他协议不受 SPI 改动影响（波形/打印正常）。

- [ ] **Step 8: spec §8 待验证项 —— 量 SPI6 时钟（master 模式）**

逻辑分析仪/示波器接 PG13 SCK，Settings 选 SPI Master 不同 baud 档位，量实际 SCK 频率。
- 若测出 ~1.875MHz（对应 baud 标签 187500）→ 实际时钟是 120MHz，**CM7 baud 标签差 10×**，记下待改
- 若测出 ~187.5kHz → 实际 12MHz，标签对的
无论哪种，prescaler 索引逻辑正确，只是显示标签可能要校准。

- [ ] **Step 9: 全部通过后更新记忆 + 提交**

更新记忆 `can-migration.md`（SPI 状态从"设计完成"→"已集成"+ commit hash）+ `cubemx-regen-fixes.md` 加第 6 项（gpio.c CS 初值靠 main.c USER CODE 2 兜底）。

最终提交（若 Task 11 修了任何验证发现的问题）：
```bash
git add -A
git commit -m "SPI: 集成完成 + 验证修复"
```

---

## Self-Review 已完成

- **Spec 覆盖**：spec §4 的 7 个文件改动 → Task 2-9 全覆盖；§3 双角色数据流 → Task 5/6；§7 验证 → Task 11；§8 待验证 → Task 11 Step 8。无遗漏。
- **类型一致**：`SPI_RangeBuffer_Read` 返回 `uint32_t` 在 Task 4(.h)/Task 5(.c)/Task 6(main 调用 `uint32_t n`)一致。`sRole` 在 hpp/cpp/applyConfig/snap/hasChanges 全接线。
- **无占位符**：所有代码块完整，无 TBD/TODO。
