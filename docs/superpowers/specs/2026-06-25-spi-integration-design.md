# SPI6 协议集成设计（CM4）

**日期**: 2026-06-25
**分支**: bare-lcd
**状态**: 设计已确认，待写实施计划
**关联**: UART/CAN/I2C 已集成（commit d688b10），SPI 是第 4 个也是最后一个协议

---

## 1. 目标

把队友 `desk-win/Protocol_Analysis` 的 `my_spi_check.c/.h` 移植到本项目 CM4，作为第 4 个协议接入双核架构：
- **CM4**：SPI6 收发 + 协议分析 → 字节进共享 ring buffer → CM7 画波形
- **CM7**：Settings 屏 SPI 配置 UI（**大部分已接好**，本次只补 Role / CS 极性两栏）+ data_screen 波形显示（已就绪）
- **双角色**：Slave（被动收外部 master）+ Master（主动时钟读外部 slave），用户在 Settings 切

## 2. 背景 / 现状

### 已就绪（不用动）
- **SPI6 CubeMX 配置**（`qiansai.ioc`）：Master/Full-Duplex/8bit/MSB/Mode0，PG13=SCK、PA7=MOSI、PG12=MISO、PG10=CS(`SPI6_CS`)，BDMA Ch0(TX)/Ch1(RX)，SPI6_IRQn/BDMA_Ch0/1_IRQn 全开（prio 5），SPI6 kernel clock = 120 MHz（D3PCLK1，.ioc 标称值，**实测见 §8 待验证项**）
- **CM7 Settings 屏 SPI 配置**（[Settings_ScreenView.cpp](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp)）：mode/datasize/baudrate/firstbit 读写 SHM_CONFIG + DCache 维护已接好（行 74-79 读、244-247 写）
- **shared_config.h**：`spi_config_t` 已存在（mode/datasize/baudrate/firstbit），`active_proto==2` 表示 SPI
- **HSEM IPC**：`shm_config_notify`（CM7）→ HSEM_ID_CONFIG → `HAL_HSEM_FreeCallback`（CM4）→ `g_config_pending` → main 循环 `apply_xxx_config_from_shm`

### 当前 SPI 占位
[main.c:196](../../../CM4/Core/Src/main.c#L196) `active_proto==2` 落到默认 UART 自环回分支，SPI 实际没接。

### 队友代码已读（`/tmp/pa_clone`，已 clone 验证）
- `my_spi_check.c` 支持 master + slave 双角色，slave 用 CS(PG10) 双边沿 EXTI + DMA 预 arm 收数据，master 用 IT 收发
- `rx_spi_buffer[256]`/`tx_spi_buffer[256]` 用 `__attribute__((section(".RAM_D3")))` —— **必须**放 D3 域（BDMA 只能访问 SRAM4@0x38000000 / 备份 RAM，不能访问 DTCM/SRAM1-3）
- `my_dma_catch.c` 是**独立**模块（TIM1+DMA 采 GPIOG->IDR 当简易逻辑分析仪），`my_spi_check.c` 只 include 未调用 —— **不移植**
- 队友 linker **没有** `.RAM_D3` 段（隐患，他们的 buffer 可能落 DTCM 导致 BDMA 失效）—— 我们必须自己加

---

## 3. 架构

### 3.1 双角色数据流

**Slave 模式**（默认，被动收外部 master）：
```
外部 master 驱动 CS↓/SCK/MOSI
   → PG10 EXTI 下降沿：记时间戳（SPI DMA 已在 init 预 arm，硬件自动收）
   → PG10 EXTI 上升沿：SPI_DMAStop_Manual + rx_spi_buffer → spi_range_buffer + 重新 arm DMA
main 循环（active_proto==2）：SPI_RangeBuffer_Read → shm_push → CM7 波形
```

**Master 模式**（主动时钟读外部 slave）：
```
main 循环（active_proto==2 && role==MASTER）：周期性 My_SPI_SendReceive(tx_pattern, rx_tmp, 5)
   → HAL_SPI_TxRxCpltCallback(master 分支)：CS 拉回 + 把 rx_tmp 字节推入 spi_range_buffer + 清 if_busy
main 循环：SPI_RangeBuffer_Read → shm_push → CM7 波形
```

两条路最后都统一从 `spi_range_buffer` 读，main 循环代码统一。

### 3.2 关键机制（队友已实现，复用）
- **Slave DMA 预 arm**：`My_SPI_Init(SLAVE)` 里直接 `HAL_SPI_TransmitReceive_DMA(...)`，不等 CS 下降沿（CS EXTI 触发再 arm 会丢数据）。CS EXTI 只做时间戳 + 帧边界。
- **CS 引脚运行时重配**：slave 模式 `CS_Switch_To_Exit()`（PG10 = 双边沿 EXTI 输入），master 模式 `CS_Switch_To_Output()`（PG10 = 推挽输出）。同一引脚双用，无硬件改动。
- **`SPI_DMAStop_Manual`**：手动停 DMA + 清 FIFO + 恢复 HAL 状态机（否则下次 HAL 调用返回 BUSY）。

---

## 4. 详细改动清单

### 4.1 `shared_config.h`（CM4 + CM7 都 include）
`spi_config_t` 加 2 个字段：
```c
typedef struct {
    uint8_t  role;        /* 新: 0=Slave(默认), 1=Master */
    uint8_t  cs_polarity; /* 新: 0=Active-Low(默认), 1=Active-High */
    uint8_t  mode;        /* 0-3 CPOL/CPHA */
    uint8_t  datasize;    /* 4-8（v1 限制，见 §8）*/
    uint32_t baudrate;    /* slave 无效；master 见 prescaler 映射 */
    uint8_t  firstbit;    /* 0=MSB, 1=LSB */
} spi_config_t;
```
默认值宏补 `SPI_DEFAULT_ROLE 0U` / `SPI_DEFAULT_CS_POL 0U`。
> sizeof 变化无影响：CM7 DCache clean/invalidate 用 `sizeof(proto_config_t)+32`。

### 4.2 CM7 Settings 屏 SPI 栏（[Settings_ScreenView.cpp](../../../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp)）
当前 SPI case（行 213）：`/* SPI 4 参数 + row5 空 */`，row1-4 = mode/datasize/baud/firstbit。

UI 只有 row1-5 共 5 行参数位，SPI 现有 4 参数（mode/datasize/baud/firstbit）+ 新增 Role = 恰好 5 个填满。**CS 极性挤不进 → v1 不做 UI，apply 端硬编码 0（低有效）**，`spi_config_t` 字段保留给 v2。

最终 SPI 栏布局：`row1=mode, row2=datasize, row3=baud, row4=firstbit, row5=Role(Slave/Master)`

改动：
- `s_data` 从 `{4,5,6,7,8,12,16}` 改 **`{4,5,6,7,8}`**（§8 #1：BDMA byte 对齐不支持 >8bit）
- 加 `sRole` 成员变量（0=Slave,1=Master）+ `+/-` 循环 + row5 显示
- `setupScreen` 读 `SHM_CONFIG->spi.role` 映射 `sRole`；`applyConfig` 写 `SHM_CONFIG->spi.role = sRole`；snap 加 `sRole`

### 4.3 `my_spi_check.c/.h`（新文件，从队友拷贝）
**拷贝来源**：`/tmp/pa_clone/CM4/Core/{Src,Inc}/my_spi_check.{c,h}` → `CM4/Core/{Src,Inc}/`

移植裁剪：
1. **删 RTOS 依赖**：`#include "cmsis_os2.h"`、`#include "FreeRTOS.h"`、`#include "FreeRTOSConfig.h"`、`#include "task.h"`（h 和 c 都删）；删底部 `My_SPI_Init_Task`/`My_SPI_Task`（c 行 469-485）
2. **删 `#include "my_dma_catch.h"`**（c 行 13，未调用，my_dma_catch 不移植）
3. **修 `analyse_index` 越界**（c 行 405、414、417 等）：EXTI 回调里 `analyse_index++` 无上界，超过 `spi_analyse[20]` 越界写。加 `if (analyse_index >= 20) analyse_index = 1;`
4. **`SPI_RangeBuffer_Read` 返回值改字节数**（c 行 120-135）：原返回 0/1(空/有数据)，改成返回实际读取字节数（`data_index`）。这是唯一一处对队友 API 的语义改动，main 循环调用更干净。
5. **master 回调补 ring 推入**：`HAL_SPI_TxRxCpltCallback` master 分支（c 行 367-370）加：用全局 `g_spi_rx_ptr`/`g_spi_rx_len`（在 `My_SPI_SendReceive` 里记）把收到的字节 `SPI_RangeBuffer_Wirte` 进 ring。`My_SPI_SendReceive`(c 行 353) 入口加 `g_spi_rx_ptr=rx_data; g_spi_rx_len=len;`。
6. **datasize 映射**：队友 `My_SPI_Init` 不暴露 datasize。在 `apply_spi_config_from_shm` 里 init 前 `hspi6.Init.DataSize = spi_datasize_from_u8(s->datasize)`（加个小映射函数，4-8 → `SPI_DATASIZE_NBIT`）。**不改队友 `My_SPI_Init` 签名**，在调用前直接赋 `hspi6.Init.DataSize`（队友 init 内部不覆盖 DataSize 字段，HAL_SPI_Init 用它）—— 待验证：队友 init 是否保留 hspi6.Init.DataSize，若被覆盖则在 My_SPI_Init 入口加赋值。

### 4.4 `CM4/stm32h747xx_flash_CM4.ld`
加 D3 域内存区 + section（rx_spi_buffer[256]+tx_spi_buffer[256]=512B，正好用 0x38000000 起的 1KB 空隙）：
```ld
MEMORY {
  ...（原有）
  RAM_D3 (xrw) : ORIGIN = 0x38000000, LENGTH = 1K
}
SECTIONS {
  ...（原有）
  .RAM_D3 (NOLOAD) :
  {
    . = ALIGN(4);
    *(.RAM_D3*)
    . = ALIGN(4);
  } >RAM_D3
}
```
> **不动 `OPEN_AMP_SHMEM`（0x38000400 起 63K）**。0x38000000-0x38000400 这 1K 原本空着。

### 4.5 `CM4/Core/Src/gpio.c` + regen 兜底
[gpio.c:49](../../../CM4/Core/Src/gpio.c#L49) CS 初始化 `GPIO_PIN_RESET` → `GPIO_PIN_SET`（空闲高，slave 模式 My_SPI_Init 会重配成 EXTI，这只影响上电瞬间）。

**regen 防护**：第 49 行不在 USER CODE 区，CubeMX regen 会重置回 RESET。在 main.c USER CODE 2 加兜底：
```c
HAL_GPIO_WritePin(SPI6_CS_GPIO_Port, SPI6_CS_Pin, GPIO_PIN_SET);
```
（仿 [cubemx-regen-fixes.md](../../../../C:/Users/admin/.claude/projects/d--CubeMx-CubeMx-project-qiansai/memory/cubemx-regen-fixes.md) 已有 regen 必查项的模式）

### 4.6 `CM4/Core/Src/main.c`
**USER CODE 2**（仿 [main.c:153](../../../CM4/Core/Src/main.c#L153) My_I2C_Init）：
```c
My_SPI_Init(0, MY_SPI_SLAVE, 0, 64, 0);  /* mode0/slave/无cs延时/prescaler64占位/CS低有效 */
HAL_GPIO_WritePin(SPI6_CS_GPIO_Port, SPI6_CS_Pin, GPIO_PIN_SET);  /* regen 兜底 */
```

**PFP 区**：加 `static void apply_spi_config_from_shm(void);` 原型

**`g_config_pending` 分支**（[main.c:173-178](../../../CM4/Core/Src/main.c#L173-L178)）加：
```c
else if (SHM_CONFIG->active_proto == 2) apply_spi_config_from_shm();
```

**`apply_spi_config_from_shm` 实现**：
```c
static void apply_spi_config_from_shm(void)
{
    spi_config_t *s = &SHM_CONFIG->spi;
    hspi6.Init.DataSize = spi_datasize_from_u8(s->datasize);  /* 4-8 → SPI_DATASIZE_NBIT */
    hspi6.Init.FirstBit = (s->firstbit) ? SPI_FIRSTBIT_LSB : SPI_FIRSTBIT_MSB;
    if (s->role == 1) {  /* Master */
        My_SPI_Init(s->mode, MY_SPI_MASTER, 0, prescaler_from_baud(s->baudrate), 0);
    } else {             /* Slave */
        My_SPI_Init(s->mode, MY_SPI_SLAVE, 0, 64, 0);
    }
    uart1_printf("[CM4] SPI reconfig OK: role=%s mode=%u ds=%u\n",
                 s->role?"MASTER":"SLAVE", s->mode, s->datasize);
}
```
`prescaler_from_baud`：CM7 档位 {187500,375000,750000,1500000,3000000,6000000} → prescaler {64,32,16,8,4,2}（slave 不用，master 用）。可放 my_spi_check.c 或 main.c。

**⚠️ buffer 生命周期**：master 模式 `My_SPI_SendReceive` 用 HAL_SPI_TransmitReceive_**IT**（非阻塞），buffer 指针在调用返回后到 TxCpltCallback 触发前一直被 DMA/IT 使用 → **tx/rx buffer 必须 file-scope static，不能用栈局部变量**（UART 的 `HAL_UART_Transmit` 是阻塞的所以能用栈，SPI 不行）。

在 USER CODE PV 加：
```c
static uint8_t spi_tx_buf[8];    /* master TX（static: IT 非阻塞需跨循环持久）*/
static uint8_t spi_rx_tmp[8];    /* master RX（TxCpltCallback 经 g_spi_rx_ptr 读它）*/
```

**主循环 SPI 数据通路**（仿 [main.c:181-206](../../../CM4/Core/Src/main.c#L181-L206) CAN/I2C 分支）：
```c
else if (SHM_CONFIG->active_proto == 2) {
    /* Master: 周期主动收发（自环回/流量发生器，v1 TX 写死递增，见 §8 #3）*/
    if (spi_deploy.spi_role == MY_SPI_MASTER) {
        if (bm_tick % 100 == 0 && if_busy == 0) {
            for (int i = 0; i < 5; i++) spi_tx_buf[i] = (uint8_t)(tx_base + i);
            My_SPI_SendReceive(spi_tx_buf, spi_rx_tmp, 5);
            tx_base += 5;
        }
    }
    /* Slave 由 CS EXTI 自动填 ring；Master 由 TxRxCpltCallback 推入 */
    uint32_t n = SPI_RangeBuffer_Read(rx_buf, sizeof(rx_buf));
    for (uint32_t i = 0; i < n; i++) shm_push(rx_buf[i]);
}
```
> `rx_buf`（读 ring 用，阻塞读完即弃）复用现有 UART 分支栈变量 [main.c:166](../../../CM4/Core/Src/main.c#L166) 的 `rx_buf[64]`，OK。`tx_base` 同理复用 [main.c:168](../../../CM4/Core/Src/main.c#L168)。**只有 master 的 spi_tx_buf/spi_rx_tmp 是新增 static**（因 IT 非阻塞）。

### 4.7 `CM4/CMakeLists.txt`
[CMakeLists.txt:60-66](../../../CM4/CMakeLists.txt#L60-L66) `target_sources` 加 `Core/Src/my_spi_check.c`。

---

## 5. 文件清单（汇总）

| 文件 | 类型 | 改动 |
|---|---|---|
| `CM4/Core/Src/my_spi_check.c` | 新 | 队友拷贝 + 6 处裁剪（§4.3） |
| `CM4/Core/Inc/my_spi_check.h` | 新 | 队友拷贝 - RTOS include |
| `CM4/Core/Inc/shared_config.h` | 改 | spi_config_t +role +cs_polarity +默认宏 |
| `CM4/stm32h747xx_flash_CM4.ld` | 改 | +RAM_D3 区 +.RAM_D3 段 |
| `CM4/Core/Src/main.c` | 改 | USER CODE 2 init + apply_spi + 主循环分支 + regen 兜底 |
| `CM4/Core/Src/gpio.c` | 改 | CS 初值 RESET→SET（regen 会被重置，靠 main.c 兜底） |
| `CM4/CMakeLists.txt` | 改 | +my_spi_check.c |
| `CM7/.../Settings_ScreenView.cpp` | 改 | s_data 砍到 ≤8 + row5=Role + sRole 读写/snap/apply |
| `CM7/.../Settings_ScreenView.hpp` | 改 | +sRole 成员 + snap 字段 |

**不动**：`qiansai.ioc`、MPU_Config（CM4 无 DCache）、CM7 main.c、data_screen、my_dma_catch。

---

## 6. CubeMX regen 影响

本次改动除 gpio.c 第 49 行（CS 电平）外，全在 USER CODE / 新文件 / linker / CMakeLists —— regen 不动。
gpio.c 第 49 行 regen 会重置回 RESET → **main.c USER CODE 2 兜底 `HAL_GPIO_WritePin(... SET)`** 必须有。
更新 [cubemx-regen-fixes.md](../../../../C:/Users/admin/.claude/projects/d--CubeMx-CubeMx-project-qiansai/memory/cubemx-regen-fixes.md) 必查清单：加第 6 项"SPI CS 初值靠 main.c USER CODE 2 兜底"。

---

## 7. 验证计划

1. **编译**：CM4 + CM7 都过（加新源文件、shared_config.h 改动）
2. **链接**：`.RAM_D3` 段生成，rx_spi_buffer/tx_spi_buffer 落在 0x38000000-0x38000400（看 map 文件）
3. **上电**：UART1 打印 `[CM4] enter USER CODE 2` 后 SPI init OK（仿 I2C/CAN）
4. **Slave 功能**：外部 SPI master 发几字节 → CM7 data_screen 看到波形/数据
5. **Master 功能**：MOSI↔MISO 短接（自环回） → data_screen 看到递增字节；或接真从机看响应
6. **切协议**：Settings 切 SPI↔UART/I2C/CAN，HSEM 重配生效，无串扰
7. **datasize**：UI 只能选 4-8（确认 12/16 不再出现）
8. **Role 切换**：Slave↔Master，CS 引脚模式跟着切（EXTI↔输出），用万用表/逻辑分析仪验 PG10

---

## 8. 已知限制 / 待验证 / 未来工作（重要 —— 防后期踩坑）

### 🔴 待验证（上电首日必须确认）
1. **datasize > 8 不支持**：CM7 `s_data` 已砍到 {4,5,6,7,8}（BDMA byte 对齐限制）。要做 >8bit 需 runtime 重配 BDMA 为 HALF_WORD 对齐 —— v2。
2. **SPI6 实际时钟**：.ioc 标称 120 MHz，但 CM7 baud 档位标签 {187500...6000000} 按 12 MHz 算的。Master 模式实际波特率可能差 10×。**用逻辑分析仪量 PG13 SCK 频率**，错了改 CM7 `s_baud` 标签值（不影响 prescaler 索引逻辑）。
3. **datasize 是否被 My_SPI_Init 覆盖**：队友 init 函数内部若重置 hspi6.Init.DataSize 则需在函数入口加赋值（§4.3 第 6 点）—— 编译后看反汇编/实测确认。
4. **`hspi6.Init.DataSize` 跨 DeInit 保留**：`My_SPI_Init` 里 `HAL_SPI_DeInit` 后 DataSize 字段是否还在（HAL DeInit 不清 Init 结构体，应保留，但确认）。

### 🟡 v1 取舍（明确不做，记下原因）
5. **Master TX 写死递增字节**：v1 master 模式只是自环回/流量发生器，**不能读真从机**（传感器/flash 需要指定寄存器命令）。v2 加 hex 输入 UI（屏幕键盘）。文档/README 须注明。
6. **CS 极性不进 UI**：`spi_config_t` 留 `cs_polarity` 字段，但 v1 硬编码 0（低有效）。绝大多数 SPI 设备低有效。v2 加 UI 栏（row 不够，需 UI 重排）。

### 🟢 跨协议待办（I2C/CAN 也有，非 SPI 独有，单独排期）
7. **协议统计没显示**：队友 `spi_analyse`（CS 脉宽/帧间隔/成功率/错误数）没传 CM7。data_screen 只画字节波形。I2C `my_i2c_analyse` / CAN `Can_Analyse` 也一样没接显示。
8. **无状态反馈**：切到 SPI slave 没接外部 master → 屏幕空白像死机；配置失败只 UART1 打印。跨协议。
9. **被动嗅探模式**：SPI 不像 I2C/CAN 能挂总线旁听。真嗅探得 `my_dma_catch`（TIM1+DMA 采 GPIOG->IDR），但它没采 SCK，只能看 pin 活动不能解码。属于另一种工具，不碰。

---

## 9. 实施顺序（writing-plans 会细化）

1. linker `.RAM_D3` + shared_config.h（基础设施）
2. my_spi_check.c/.h 移植裁剪
3. main.c 接线（init + apply + 主循环 + regen 兜底）+ CMakeLists + gpio.c
4. CM7 Settings（s_data 砍 + Role 栏）
5. 编译 → 上电 → §7 验证 → 提交
