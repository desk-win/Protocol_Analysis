# 千赛 协议分析仪 — 项目架构说明

> 给队友快速了解整机功能 + 架构。**双核通信部分重点看 §3**(最容易踩坑)。
> 维护者:代码里 USER CODE 区的注释和这里的描述是一一对应的,改一处记得同步另一处。

---

## 1. 这是什么

一块基于 **STM32H747** 双核的"协议分析仪/检测仪":接各种串行协议(UART/SPI/I2C/CAN)的信号,**实时画时序波形** + **录制到 SD** + **回放**,带触摸屏(TouchGFX)配参数。队友 `desk-win/Protocol_Analysis` 的协议解析代码移植到 CM4,屏幕和系统是本项目自己写的。

**一句话**:外部信号 → CM4 抓 → 共享内存 → CM7 TouchGFX 画波形 + 存 SD。

---

## 2. 硬件 / 引脚

| 外设 | 引脚 | 用途 |
|---|---|---|
| **USART6** | PG14(TX)/PG9(RX) | UART 协议口,**默认自环回**(PG14→PG9 短接,自发自收演示)|
| **USART1** | PA9/PA10 | 调试 printf(电脑 USB 转串口看 `[CM4]` 日志)|
| **SPI6** | PG13=SCK / PA7=MOSI / PG12=MISO / PG10=CS | SPI 协议口,BDMA |
| **I2C4** | PD12=SCL / PD13=SDA | I2C 协议口 |
| **FDCAN1** | PD0/PD1 | CAN 协议口 |
| **SDMMC2** | SD NAND | 录制文件存储(FatFs)|
| **LTDC + SDRAM(FMC)** | — | TouchGFX framebuffer(7" 屏 800×480)|
| **触摸** | PB10/PB11(软件 I2C)| GT9xxx 触摸 |

**时钟**:SYSCLK 480MHz,HCLK 240MHz。SPI6 在 D3 域 = 120MHz(HCLK/2)。FDCAN kernel(PLL1Q)= 240MHz。

---

## 3. 双核架构(重点)

### 3.1 谁干什么

| 核 | 跑什么 | 干什么 |
|---|---|---|
| **CM7** | FreeRTOS + TouchGFX | 画 UI、读共享内存画波形、SD 读写(录制/回放/文件列表)、Apply 时写配置 + 通知 CM4 |
| **CM4** | **裸机**(无 FreeRTOS) | 抓协议信号(UART/SPI/I2C/CAN)、协议解析、把字节推进共享内存、收 CM7 的重配通知改外设 |

> **CM4 为什么裸机?** FreeRTOS 在 CM4 上 SysTick 不工作 + `HAL_Delay` 在 task 里会卡住(实测卡 193)。所以 CM4 是一个 `for(;;)` 裸循环,`osKernelStart()` 永不执行。详见 `CM4/Core/Src/main.c` USER CODE 2。

### 3.2 共享内存(两块,都在 SRAM1 @ D2 域)

物理上一块 SRAM1(0x30000000 起),逻辑分两区,都配 MPU **non-cacheable + shareable**(coherency):

#### ① 字节环形缓冲(ring buffer)— 数据通路
```
地址 0x30000000,结构 shared_ring_t (shared_buf.h):
  head (u16)   — CM4 写位置(仅 CM4 推进)
  tail (u16)   — CM7 读位置(仅 CM7 推进)
  data[2048]   — 字节流
```
- **CM4 抓到字节 → `shm_push(byte)`** 写 head(满则覆盖最老,保证最新不丢)
- **CM7 周期 invalidate DCache + 读 SHM_RING** → 画波形

#### ② 协议配置(config)— 控制通路
```
地址 0x30001000,结构 proto_config_t (shared_config.h):
  active_proto (u8)   — 1=UART/2=SPI/3=I2C/4=CAN,当前选中
  uart / spi / i2c / can — 各协议参数(baud/databits/mode/...)
```
- **CM7 Apply → 写 SHM_CONFIG + clean DCache + HSEM 通知** → CM4 读 → 重配外设
- 留 ~2KB 间距到 ring buffer(防 ring 溢出污染 config)

### 3.3 ⚠️ 方向性限制(最重要的一条)

> **CM4 → CM7 通;CM7 → CM4 不通。**

| 方向 | 通不通 | 为什么 |
|---|---|---|
| CM4 写 → CM7 读 | ✅ | CM4 **无 DCache**,写直达物理 SRAM;CM7 读前 `SCB_InvalidateDCache_by_Addr` 拿新值 |
| CM7 写 → CM4 读 | ❌ | CM7 有 DCache,写停在 cache 里;`SCB_CleanDCache` 也 flush 不到 CM4 能稳定看到的物理(H7 无双核硬件 coherency) |

**后果**:
- `shm_init()`(清 ring)必须 **CM4 调**,不是 CM7(CM7 写的 head/tail 到不了 CM4)
- CM7 消费 ring 用**本地 `read_pos`**,不写共享 tail(写了 CM4 也看不到,反而 ring 满逻辑会乱)
- CM4 不会读 CM7 的任何东西;CM4 的状态(如 `spi_deploy.spi_role`)只能 CM4 自己用

### 3.4 双核启动同步(HSEM ID=0)

HSEM(硬件信号量)是双核唯一的"叫醒对方"机制。

```
CM7 先跑(系统时钟/SDRAM(FMC)/LTDC 初始化)
   ↓ Release HSEM_ID_0
CM4 一直在 STOP 等这个信号 → 醒来 → HAL_Init + 配外设(UART6/SPI6/I2C4/FDCAN1)
```
`HSEM_ID_0` **被 boot 同步占用**,别处不能再用。`main.c` 里的 `DUAL_CORE_BOOT_SYNC_SEQUENCE` 宏控制这段。

### 3.5 配置 IPC(HSEM ID=1)— Apply 全流程

用户在屏幕改协议参数 → Apply → CM4 外设跟着变,走这条路:

```
[CM7] Settings 屏 onApplyClick:
  applyConfig() 写 SHM_CONFIG->各字段
  SCB_CleanDCache_by_Addr(SHM_CONFIG)      ← 强制 CM7 cache 刷到物理 SRAM
  shm_config_notify():
      HAL_HSEM_Take(HSEM_ID_1, 0) + __DSB  ← 拿信号量(等 CM4 释放)
      HAL_HSEM_Release(HSEM_ID_1, 0)       ← 释放 = 给 CM4 发"配置就绪"中断
                    │
                    ▼ (硬件触发 CM4 HSEM 中断)
[CM4] HAL_HSEM_FreeCallback(HSEM_ID_1):
  g_config_pending = 1                     ← 只置 flag,不在这改外设(避免和 main 循环竞态)
  HAL_HSEM_ActivateNotification(HSEM_ID_1) ← ⚠️ 必须重新激活,否则第二次不通知!

[CM4] main 裸循环下一轮:
  if (g_config_pending) {
      读 SHM_CONFIG->active_proto
      switch: apply_uart/spi/i2c/can_config_from_shm()  ← 真正改外设寄存器
  }
```

**关键点**:
- **不在中断里改外设**:UART/SPI/CAN 的重配要 DeInit+Init,和 main 循环里正在用的 DMA/收发会竞态。所以中断只置 flag,主循环统一改。
- **必须重新 ActivateNotification**:HSEM FreeCallback 触发后 IER 标志会被清,不重新激活,第二次 Apply CM4 收不到。
- **DCache 维护**:CM7 写完 SHM_CONFIG 必须 `Clean`(刷到物理),否则 CM4 读到旧值。MPU non-cacheable 历史没完全生效,所以靠手动 Clean/Invalidate 绕过(`Settings_ScreenView.cpp` + `data_screen`)。

### 3.6 完整数据流(以 SPI 为例)

```
外部 SPI master 驱动 CS(PG10)↓/SCK(PG13)/MOSI(PA7)
   ↓ SPI6 从机(BDMA 预 arm,见下)收进 rx_spi_buffer[256] @ RAM_D3
   ↓ CS↑ EXTI → SPI_DMAStop_Manual + 搬进 spi_range_buffer (my_spi_check.c)
[CM4] main 循环 (active_proto==2):
   SPI_RangeBuffer_Read → 每个 byte shm_push → SHM_RING (0x30000000)
   ↓
[CM7] freertos defaultTask (每 100ms):
   SCB_InvalidateDCache_by_Addr(SHM_RING)   ← 拿 CM4 写的新数据
   读字节 → 累积到 SD 录制缓冲(若在录)+ 本地 read_pos 推进
   ↓
[CM7] data_screen handleTickEvent:
   从 SHM_RING 读最近 N 字节 → waveWidget.setBytes → 画波形
```

### 3.7 各协议 CM4 怎么收

| 协议 | CM4 收法 | 数据来源 |
|---|---|---|
| UART | USART6 DMA + IDLE 空闲中断,`my_uart_check.c` 收进 ring | 外部 TX 或自环回 |
| SPI | SPI6 **从机**,CS(PG10)双边沿 EXTI + **BDMA 预 arm**(init 时就开 DMA,不等 CS,否则丢数据),`my_spi_check.c` | 外部 master |
| I2C | I2C4 **从机**(`MY_I2C_SLAVE`,监听 own_address=0x50),地址匹配中断收,`my_i2c_check.c` | 外部 master |
| CAN | FDCAN1 RxFifo0 中断,`my_can_check.c` 收帧入 ring | CAN 总线 |

> **SPI 从机 DMA 预 arm**:SPI 从机不能等 CS 下降沿再开 DMA(中断延迟会丢首字节)。所以 init 就开 `HAL_SPI_TransmitReceive_DMA`,CS EXTI 只做时间戳 + 帧边界(CS↑ 时停 DMA + 搬数据 + 重 arm)。
> **SPI BDMA 限制**:BDMA 只能访问 D3 域 SRAM4(0x38000000),所以 `rx_spi_buffer`/`tx_spi_buffer` 用 `__attribute__((section(".RAM_D3")))`,linker 有对应的 `RAM_D3` 段(0x38000000,1KB)。

---

## 4. 4 个协议的设置参数

详细对照表见 [docs/protocol-settings-reference.md](docs/protocol-settings-reference.md)。速查:

| 协议 | active_proto | 可配参数 | CM4 默认 |
|---|---|---|---|
| UART | 1 | baud(9600~921600)/data(7/8/9)/stop(1/2)/parity(None/Even/Odd)/flow(None/RTS/CTS/RTS-CTS) | 115200 8N1 |
| SPI | 2 | mode(0-3 CPOL/CPHA)/data(4-8)/baud(120MHz 分频)/first(MSB/LSB)/role(Slave/Master) | Slave mode0 8bit |
| I2C | 3 | clock(100k/400k/1M)/addr(7/10-bit)/Slave(0x50 固定,只读显示) | 从机 0x50 100k |
| CAN | 4 | baud(50k~1M)/mode(Normal/Loopback/Silent/LB+Silent)/BRP(自动算显示) | 500k Normal |

**SPI datasize 砍到 ≤8**:BDMA byte 对齐,12/16bit 会错位。CAN BRP = 240MHz/(baud×16),屏幕会显示算出的值。

---

## 5. TouchGFX 屏幕(CM7)

| 屏 | 干啥 |
|---|---|
| **start_screen** | 主菜单,选协议进 data_screen,或进 SD 文件管理 |
| **data_screen** | 实时时序波形(`WaveformWidget`)+ 4 协议按钮(开始/停止录制)+ 回放控制(Run/Pause/Prev/Next/Stop/进度条)|
| **settings_screen** | 选协议 + 改参数 + Apply/Discard/Cancel(改了参数点 back 弹确认)|
| **SD_Test_Screen** | SD 文件列表(刷新/删除/选文件回放)|

**波形控件**(`WaveformWidget`):UART 画完整 framing(起始位红/数据位绿/校验黄/停止位蓝),SPI/I2C/CAN 只画数据位(无 framing)。`applyWaveConfig()` 按当前协议(回放时按录制时配置)设参数。

> ⚠️ TouchGFX 字体 `WildcardCharacterRanges="0x20-0x7E"`(只 ASCII),动态文本中文会显示 `?`。所以屏幕上的参数提示都是英文。

---

## 6. 录制 / 回放系统(CM7 freertos.c)

### 录制
- 点协议按钮 → `g_record_req=N` → freertos 开 `d_<proto>_<seq>.log`(seq 1~3)
- **挑槽位**:`f_stat` 扫 3 个槽,**空槽优先;全满覆盖 mtime 最旧**的(真保留最近 3 个,不盲目轮转)
- **文件格式**:`[4B magic "RECL"][~40B proto_config_t 全部配置][波形字节...]`
  - magic + config 是 header,录制开始时先写(把当时的整机配置存进去)
- 波形字节 512B 块写,每 10 秒 flush + sync
- 停止:Save(保留 f_close)/ Discard(删 f_unlink)/ Cancel(继续录)

### 回放
- 文件列表选文件 → Play → freertos `f_open` 读
- 先读 magic:匹配 → 读 proto_config_t 到 `g_playback_cfg` + 波形从 header 后开始;老文件(无 magic)→ 从头当波形(向后兼容)
- 所有 seek 偏移 header_len;`file_size` 减 header(进度条按波形净大小)
- `data_screen` 进回放 → `applyWaveConfig(&g_playback_cfg)` → **按录制当时的配置画 framing**(改了当前配置也不影响回放)

---

## 7. SD NAND 注意点

- 偶发读写 DISK_ERR(SD NAND 特性),所有 `f_open`/`f_write`/`f_stat`/`f_opendir` 都**重试 3 次**
- FatFs `_FS_NOFSINFO=3`(强制扫真实 FAT,治 Free 显示假满)
- `sd_diskio.c` 用 polling + 重试(不是 DMA RTOS 模板,regen 会改回去,见下)
- `HardwareFlowControl=ENABLE`(双核下 DISABLE 会 FIFO 溢出)

---

## 8. 构建 / 烧录

- 工具链:`gcc-arm-none-eabi.cmake`(STM32CubeCLT),CMake
- 两个独立构建目录:`CM4/build/`、`CM7/build/`
  ```bash
  cd CM4/build && cmake --build .
  cd CM7/build && cmake --build .
  ```
- 产物:`CM4/build/qiansai_CM4.elf` + `CM7/build/qiansai_CM7.elf`,双核分别烧
- CubeMX regen 后**必查 6 项**(见 [cubemx-regen-fixes 记忆](C:/Users/admin/.claude/projects/d--CubeMx-CubeMx-project-qiansai/memory/cubemx-regen-fixes.md)):TouchGFX init 重复、task stack、MPU region、ffconf、sd_diskio、SPI CS 电平

---

## 9. 已知限制 / 待办(v2)

- SPI datasize ≤8(BDMA byte 对齐);要 >8bit 需 runtime 改 BDMA 为 HALF_WORD
- SPI Master 模式 TX 写死递增字节(自环回演示);要发指定命令读寄存器,得加 hex 输入 UI
- CS 极性、I2C own_address:UI 没暴露(I2C 地址屏幕只读显示 0x50),要改改代码默认值
- 协议统计(CS 脉宽/帧间隔/成功率/错误数)队友代码里有,但没传到屏幕显示
- 配置不持久(断电丢):Apply 只写易失 SHM_CONFIG,没存 SD/flash。要持久得加开机 f_read + Apply 时 f_write(见 [docs/superpowers/specs/2026-06-25-recording-config-header.md](docs/superpowers/specs/2026-06-25-recording-config-header.md) 的讨论)

---

## 10. 关键文件速查

| 文件 | 干啥 |
|---|---|
| `CM4/Core/Src/main.c` | CM4 裸机 main 循环 + apply_xxx_config_from_shm + HSEM 回调 |
| `CM4/Core/Src/my_{uart,spi,i2c,can}_check.c` | 队友移植的 4 协议解析 |
| `CM4/Core/Inc/shared_config.h` | 共享内存结构 + HSEM ID + REC_MAGIC(**两核各一份,必须同步**)| 
| `CM4/Core/Inc/shared_buf.h` | ring buffer 结构 + shm_push/shm_pop |
| `CM4/stm32h747xx_flash_CM4.ld` | linker,RAM_D3 段(SPI BDMA buffer)|
| `CM7/Core/Src/main.c` | CM7 入口 + `shm_config_notify`(HSEM 通知 shim)+ 全局变量 |
| `CM7/Core/Src/freertos.c` | defaultTask:SD 录制/回放/文件列表 + 读 SHM_RING |
| `CM7/TouchGFX/gui/src/*/Settings_ScreenView.cpp` | 设置屏(参数配置 + Apply)|
| `CM7/TouchGFX/gui/src/*/Data_screenView.cpp` | 数据屏(实时波形 + 回放)|
| `docs/superpowers/specs/` | 设计文档(SPI 集成、录制 header)| 
| `docs/superpowers/plans/` | 实施计划 |
| `docs/protocol-settings-reference.md` | 4 协议参数对照表 |

---

## 关联文档
- [docs/protocol-settings-reference.md](docs/protocol-settings-reference.md) — 4 协议每个参数的含义
- [docs/superpowers/specs/2026-06-25-spi-integration-design.md](docs/superpowers/specs/2026-06-25-spi-integration-design.md) — SPI 集成设计(含双角落数据流)
- [docs/superpowers/specs/2026-06-25-recording-config-header.md](docs/superpowers/specs/2026-06-25-recording-config-header.md) — 录制配置 header
