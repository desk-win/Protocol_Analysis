# 多协议总线检测工具 — 代码说明文档

> **适用平台：** STM32H747（Cortex-M7 + M4 双核）  
> **使用外设：** I2C4 / SPI6 / USART6 / FDCAN1  
> **文档目的：** 帮助队友快速了解各模块的设计思路与函数接口

---

## 总体架构

项目包含四个独立的协议检测模块，每个模块结构相似，均分为以下四层：

| 层次 | 内容 |
|------|------|
| 工具函数层 | 环形缓冲区的读写管理 |
| 初始化 / 参数配置层 | 外设初始化、运行时参数变更 |
| 收发层 | 单次发送、循环发送、中断回调接收 |
| 协议分析层 | 错误统计、帧分析、总线状态评估 |

---

## 一、UART 模块（`my_uart_check.c`）

使用 **USART6 + DMA + 空闲中断（IDLE）** 实现不定长数据收发。

### 全局变量

| 变量名 | 类型 | 说明 |
|--------|------|------|
| `now_uart_state` | `My_UART_State` | 收发通知标志（`rx_notice` / `tx_notice`） |
| `cycle_tx_ctrl` | `UART_Cycle_Tx_t` | 循环发送控制块（数据指针、间隔、剩余次数） |
| `rx_ring_buf` | `RingBuffer_t` | 接收环形缓冲区 |
| `rx_uart_buffer` | `uint8_t[]` | DMA 搬运数据的中间缓冲区 |
| `uart_errors` | `UART_ErrorStats_t` | 各类硬件错误计数及滑动窗口错误率 |
| `uart_analysis` | `UART_Pact_t` | 协议层分析数据（帧统计、帧间隔、帧头帧尾） |

---

### 工具函数

#### `UART_RingBuffer_Push(uint8_t data)`
将一个字节写入接收环形缓冲区。缓冲区满时自动覆盖最老的数据（tail 前移），保证最新数据不丢失。

#### `My_UART_Read_RingBuffer(uint8_t *pDest, uint32_t len)`
从环形缓冲区读取最多 `len` 个字节到 `pDest`，返回实际读取长度。供应用层在 `rx_notice` 置 1 后调用取出数据。

---

### 初始化 / 参数配置

#### `My_UART_Init()`
初始化所有全局状态结构体并启动 **DMA + 空闲中断接收**（`HAL_UARTEx_ReceiveToIdle_DMA`）。需在进入主循环前调用一次。

#### `UART_Param_Change(baud, data_len, stop_bits, parity, flow_ctrl)`
运行时修改 UART 参数（波特率、数据位、停止位、校验位、硬件流控）。  
流程：停止 DMA → 反初始化 → 写入新参数 → 重新初始化 → 重启 DMA 接收。  
返回 `HAL_OK` / `HAL_ERROR`。

---

### 收发

#### `My_UART_Send_Single(uint8_t *tx_data, uint32_t len)`
单次发送。会先等待上一次发送完成（轮询 `gState`），同时强制停止循环发送，然后通过中断方式发送。

#### `My_UART_Send_Cycle(uint8_t *tx_data, uint32_t len, uint16_t occur, uint32_t times)`
配置循环发送参数：数据指针、长度、发送次数 `occur`、间隔时间 `times`（ms）。配置完成后需在主循环中持续调用 `My_UART_LoopTask()` 才会实际发送。

#### `My_UART_LoopTask()`
**需放在 while 主循环中调用。** 根据时间戳判断间隔是否到达，若到达且串口空闲则触发一次中断发送，次数用完后置 `tx_notice = 1` 通知应用层。

#### `HAL_UARTEx_RxEventCallback(huart, Size)` *(HAL 回调)*
DMA + 空闲中断触发后自动调用。将本次接收的 `Size` 个字节逐字节压入环形缓冲区，置 `rx_notice = 1`，更新帧间隔统计（最大/最小/平均），并进行可选的帧头帧尾校验。完成后重启 DMA 接收。

#### `HAL_UART_TxCpltCallback(huart)` *(HAL 回调)*
发送完成中断回调。若当前不处于循环发送模式，则置 `tx_notice = 1` 通知应用层发送已完成。

---

### 错误检测 / 协议分析

#### `Update_Error_Window(uint8_t if_error)`
维护一个大小为 `ERROR_WINDOW_SIZE` 的滑动窗口，记录最近 N 次收发是否有错误，并实时计算近期错误率 `uart_errors.recent_error_rate`。

#### `HAL_UART_ErrorCallback(huart)` *(HAL 回调)*
HAL 库检测到硬件错误时自动调用。分别统计帧错误（FE）、奇偶校验错误（PE）、溢出错误（ORE）、噪声错误（NE）的累计计数，并更新滑动窗口。最后强制重启 DMA 接收（HAL 库发生错误后会关闭接收）。

#### `Check_Form(uint8_t *data, uint32_t len, UART_Pact_t form_data)`
校验数据帧的帧头与帧尾是否与配置结构体中设定的值匹配。匹配返回 1，不匹配返回 0。仅在 `uart_analysis.if_form == 1` 时由回调自动调用。

---

## 二、I2C 模块（`my_i2c_check.c`）

使用 **I2C4 + 中断** 实现主机模式（扫描/读写）和从机模式（不定长接收），并通过 DWT 计时器精确测量时钟拉伸时长。

### 全局变量

| 变量名 | 类型 | 说明 |
|--------|------|------|
| `my_i2c_data` | `I2C_Data` | 主从收发缓冲区、传输模式标志 |
| `my_i2c_deploy` | `I2C_Deploy` | 当前配置（主从模式、地址、频率） |
| `my_i2c_analyse` | `I2C_Analyse` | 协议层统计（帧数、成功率、时钟拉伸） |
| `now_frame` | `I2C_This_Frame` | 当前帧的临时分析数据 |
| `i2c_rangebuffer` | `I2C_Range_Buffer` | 接收数据的环形缓冲区 |

---

### 工具函数

#### `I2C_RangeBuffer_Write(uint8_t data)`
向接收环形缓冲区写入一个字节，缓冲区满时覆盖最老数据，返回是否发生覆盖（1 = 有覆盖）。

#### `I2C_RangeBuffer_Read(uint8_t *data, uint32_t data_len)`
从环形缓冲区读取最多 `data_len` 个字节，返回 1 表示正常读取，0 表示缓冲区为空。

#### `I2C_PutData_To_Buffer()`
将 `my_i2c_data` 中本次接收到的数据批量写入环形缓冲区，主从模式下分别读取对应缓冲区。

---

### 初始化 / 参数配置

#### `I2C_CalcTiming(uint32_t target_hz)`
根据目标 SCL 频率和内核时钟（120 MHz）自动计算 I2C Timing 寄存器值，遍历 PRESC 找到满足时序约束的分频组合，并根据速率段选取合适的 SCLDEL / SDADEL 经验值。返回 Timing 寄存器原始值，失败返回 0。

#### `My_I2C_Init(now_mode, address_mode, address_7bit, address_10bit, stretch, scl_fq)`
完整初始化 I2C4 外设，可配置：主从模式、7/10 位寻址、时钟拉伸开关、SCL 频率。作为从机时自动开启地址监听中断（`HAL_I2C_EnableListen_IT`）；作为主机时自身地址填 0。

#### `Switch_I2C_Mode(I2C_Mode now_mode)`
在运行时切换 I2C 主从角色。切换到从机时开启监听中断，切换到主机时关闭监听中断。

---

### 主机模式收发

#### `My_I2C_Master_ScanBus()`
对总线进行全地址扫描（7 位扫 0x00~0x7F，10 位扫 0x000~0x3FF），每个地址尝试 HAL_I2C_IsDeviceReady，有响应则在 `bit7_check[]` 或 `bit10_check[]` 对应位置置 1。适用于上电时探测总线上有哪些从机。

#### `My_I2C_Master_Send_Simple(address, data, len)`
中断方式向指定从机地址发送数据，同时记录起始时间戳（用于后续计算时钟拉伸）。

#### `My_I2C_Master_Read_Simple(address, data, len)`
中断方式从指定从机地址读取数据，记录起始时间戳。

#### `My_I2C_Master_Send_ReStart(uint16_t address)`
发起 **Write-then-Read 顺序传输**（不释放总线）：先调用 `HAL_I2C_Master_Sequential_Transmit_IT` 发送寄存器地址，发送完成后在 `HAL_I2C_MasterTxCpltCallback` 里自动衔接 `HAL_I2C_Master_Sequential_Receive_IT` 读取数据，全程不产生 STOP 信号，符合标准 I2C 写后读时序。

#### `HAL_I2C_MasterTxCpltCallback` / `HAL_I2C_MasterRxCpltCallback` *(HAL 回调)*
主机发送完成时：若为顺序传输模式则启动读取阶段；若为普通发送则计算时钟拉伸。主机接收完成时：计算时钟拉伸，更新帧字节数，调用 `Frame_Analyse()` 和 `I2C_PutData_To_Buffer()`。

---

### 从机模式收发

#### `HAL_I2C_AddrCallback(hi2c, TransferDirection, AddrMatchCode)` *(HAL 回调)*
硬件完成地址比对后触发。若主机要写入（Transmit 方向），从机进入接收模式，记录 START 时间戳，复位帧数据，并开启逐字节顺序接收中断；若主机要读取（Receive 方向），从机进入发送模式，开启顺序发送中断。

#### `HAL_I2C_SlaveRxCpltCallback(hi2c)` *(HAL 回调)*
每接收一个字节触发一次。累计字节数，记录第一个字节到达时刻，缓冲区未满则重新开启接收等待下一字节，缓冲区满则先写入环形缓冲区再重新开启接收。

#### `HAL_I2C_ListenCpltCallback(hi2c)` *(HAL 回调)*
接收到 STOP 位时触发，标志一帧传输结束。计算 START 到第一数据字节的延迟，调用 `Frame_Analyse()` 完成帧统计，然后重新开启 `EnableListen_IT` 等待下一帧。

---

### 错误检测 / 协议分析

#### `Record_Stretch()`（内部函数）
在传输完成回调中调用，通过比较实际耗时与理论无拉伸时长（`len × 9 / SCL_freq`）计算本次时钟拉伸时长，并更新最大值和累计均值。

#### `Frame_Analyse()`（内部函数）
每帧结束时调用，根据 `error_code` 判断本帧是否成功，累计总帧数、成功帧数、失败帧数、总字节数，并计算传输成功率。

#### `Frame_Reset()`（内部函数）
在新一帧开始前（`AddrCallback` 内）调用，清零当前帧的临时统计数据和错误码。

#### `My_I2C_Bus_Check()`
总线死锁检测与恢复。在 I2C 处于忙状态且 SCL/SDA 被拉低时，判定为死锁：将 SCL 配置为 GPIO 推挽输出，手动发出 9 个时钟脉冲，每发一个检测 SDA 是否释放，最后手动产生 STOP 信号，再重新初始化 I2C 外设。

#### `HAL_I2C_ErrorCallback(hi2c)` *(HAL 回调)*
分类统计 ACK 失败（AF）、总线错误（BERR）、仲裁丢失（ARLO）、超时（TIMEOUT）、溢出（OVR）的累计次数，并更新 `error_code` 位标志。从机模式下错误后自动重启监听中断。

---

## 三、SPI 模块（`my_spi_check.c`）

使用 **SPI6 + DMA**，软件 CS（GPIO_PIN_10 / GPIOG），支持主机和从机两种角色。

### 全局变量

| 变量名 | 类型 | 说明 |
|--------|------|------|
| `rx_spi_buffer / tx_spi_buffer` | `uint8_t[]` | DMA 收发缓冲区 |
| `if_busy` | `uint8_t` | 主机发送忙标志 |
| `if_rx_finish` | `volatile uint8_t` | 从机一帧接收完成标志 |
| `spi_error_code` | `My_SPI_Error` | 各类错误计数 |
| `spi_deploy` | `My_SPI_Deploy` | 当前 SPI 配置（模式、CS 延时、分频） |
| `rx_range_buffer` | `SPI_Range_Buffer` | 接收数据环形缓冲区 |
| `spi_analyse` | `My_SPI_Analyse` | 协议层分析（CS 宽度、帧间隔、成功率） |

---

### 工具函数

#### `CS_Pin_State(uint8_t level)`
控制软件 CS 引脚电平，1 = 高电平，0 = 低电平。仅在主机模式下使用。

#### `SPI_RangeBuffer_Wirte(uint8_t data)`
向接收环形缓冲区写入一个字节，满时覆盖旧数据，返回是否发生覆盖。

#### `SPI_RangeBuffer_Read(uint8_t *data, uint32_t data_len)`
从环形缓冲区读取最多 `data_len` 个字节，0 = 缓冲区为空，1 = 正常读取。

#### `SPI_PutData_To_Buffer()`
检查 `if_rx_finish` 标志，若置 1 则通过 DMA 计数器计算本帧实际接收长度，将数据批量写入环形缓冲区后清零标志。**需在主循环中轮询调用**（从机模式）。

---

### 初始化 / 参数配置

#### `SPI_Time_Mode(uint8_t mode)`
配置 SPI 时钟极性（CPOL）和相位（CPHA），支持模式 0~3，直接写入 `hspi6.Init`，需配合初始化或重新 Init 使用。

#### `SPI_BaudRatePrescaler(uint16_t prrscaler)`
将整数分频值（2/4/8/.../256）映射为 HAL 库宏并写入 `hspi6.Init.BaudRatePrescaler`。

#### `CS_Switch_To_Output()`
关闭 CS 引脚的外部中断，将 CS 引脚配置为推挽输出（主机模式使用）。

#### `CS_Switch_To_Exit()`
将 CS 引脚配置为双边沿触发的外部中断输入（从机模式使用），并开启 NVIC 中断。

#### `My_SPI_Init(time_mode, role, cs_delay, baudrate, cs_polarity)`
SPI 完整初始化入口。主机模式下配置全部参数（CPOL/CPHA、分频、CS 极性、CS 延时），将 CS 引脚配置为推挽输出。从机模式下固定 CPOL=0/CPHA=0，预先开启 DMA 全双工收发后立即关闭 SPI 外设，等待外部中断（CS 下降沿）触发后再开启，以避免上升沿触发时漏掉第一个字节。

#### `Switch_SPI_Mode(My_SPI_Mode spi_mode)`
停止 DMA 并反初始化后切换主从角色，重新初始化 SPI。从机模式下同样预开启 DMA 后关闭外设。

---

### 主机模式收发

#### `My_SPI_Send(uint8_t *data, uint32_t len)`
主机单向发送：拉低 CS → 中断方式发送 → 发送完成回调中拉高 CS 并清除忙标志。同一时间只允许一次发送（`if_busy` 互锁）。

#### `My_SPI_SendReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t len)`
主机全双工收发：拉低 CS → 中断方式同时发送 MOSI 并采集 MISO → 完成回调中拉高 CS。实际使用中可用此函数替代 `My_SPI_Send`，因为 SPI 是全双工总线。

#### `HAL_SPI_TxCpltCallback` / `HAL_SPI_TxRxCpltCallback` *(HAL 回调)*
发送完成或收发完成时，主机模式下拉高 CS 并清除忙标志；从机模式下重新开启 DMA 收发（用于 DMA 缓冲区溢出后续帧的接收）。

---

### 从机模式收发

#### `HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)` *(HAL 回调)*
CS 引脚双边沿外部中断：
- **下降沿**（CS 有效）：记录当前时刻（DWT），开启 SPI 外设开始接收。
- **上升沿**（CS 无效）：停止 DMA，计算 CS 脉冲宽度，累计帧数与成功/失败帧数，计算传输成功率和帧间隔，将 `if_rx_finish` 置 1，然后预开启下一次 DMA 并关闭外设等待下一帧。

---

### 错误检测 / 协议分析

#### `HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)` *(HAL 回调)*
分类统计 MODF（模式错误）、OVR（溢出）、DMA 错误、CRC 错误的累计次数，并记录原始 `error_code`。

**协议层可读数据（直接读全局变量）：**

| 字段 | 含义 |
|------|------|
| `spi_analyse.cs_gap_tick` | 上一帧 CS 脉冲宽度（µs） |
| `spi_analyse.spi_frame_gap` | 帧间隔（µs） |
| `spi_analyse.transmit_success_rate` | 传输成功率（0.0~1.0） |

---

## 四、CAN 模块（`my_can_check.c`）

使用 **FDCAN1**（经典 CAN 模式），支持标准帧/扩展帧/遥控帧收发，双 FIFO 接收，完整总线状态机监控。

### 全局变量

| 变量名 | 类型 | 说明 |
|--------|------|------|
| `my_can_rangebuffer` | `Can_Range_Buffer` | 接收报文环形缓冲区 |
| `TxHeader` | `FDCAN_TxHeaderTypeDef` | 发送报文头（ID、帧类型、DLC 等） |
| `my_can_cycle` | `Can_Cycle` | 循环发送控制块 |
| `rx_message` | `Can_Message_Struct` | 接收报文解析结果 |
| `my_can_judge` | `Can_Analyse` | 错误统计与协议分析数据 |

---

### 工具函数

#### `CAN_RangeBuffer_Write(rangebuffer, rx_mes)`
将一个 `Can_Message_Struct` 写入环形缓冲区，缓冲区满时覆盖最老帧并返回 0（正常返回 1）。

#### `CAN_RangeBuffer_Read(rangebuffer, rx_mes)`
从环形缓冲区读出一帧报文，缓冲区将空时返回 0。

#### `Switch_Len_TO_DLC(uint8_t len)` / `Switch_DLC_TO_Len(uint32_t DLC)`
字节长度与 DLC 字段之间的互转工具函数。

---

### 初始化 / 参数配置

#### `CAN_Handler_Start()`
开启全部需要监听的 FDCAN 中断通知（错误警告、错误被动、总线关闭、FIFO 丢帧、FIFO 新消息、发送完成）并初始化总线状态为主动错误状态。**需在 HAL 完成初始化后调用。**

#### `CAN_Param_Change(baud, userBRP, userBS1, userBS2, userSJW, mode, autosend)`
运行时修改 CAN 参数：停止外设 → 写入新的 BRP/BS1/BS2/SJW（决定波特率和采样点）、工作模式（正常/静默/回环）、自动重传使能 → 重新初始化并启动，重新激活发送完成和接收中断。波特率计算公式：`Baud = CLK / (BRP × (1 + BS1 + BS2))`。

---

### 收发

#### `My_CAN_Send_Single_Init(id, idtype, frametype, dlc)`
配置发送报文头参数（ID 值、标准帧/扩展帧、数据帧/遥控帧、数据长度），**必须在首次发送前调用一次**，后续格式不变则无需重复调用。

#### `My_CAN_Send_Single(uint8_t *data)`
将数据写入 FDCAN Tx FIFO，由硬件按优先级排队发送，完成后触发 `HAL_FDCAN_TxEventFifoCallback`。调用前需先调用 `My_CAN_Send_Single_Init`。

#### `My_CAN_Send_Cycle(uint8_t *data)`
**需放在 while 主循环中调用。** 按照 `my_can_cycle.interval` 设定的间隔周期性调用 `My_CAN_Send_Single`，未使能（`enable == 0`）时直接返回。

#### `CAN_Filter_Config(mode, targetID, idType, targetFIFO)`
配置接收过滤器：`mode=0` 为全通模式（接收所有 ID），`mode=1` 为精准匹配指定 ID。可选择将匹配报文路由到 FIFO0 或 FIFO1。

#### `CAN_Rx_Deal(hfdcan, RxFifo)`
接收处理核心函数，由两个 FIFO 回调共同调用。从硬件取出报文，解析 ID、ID 类型、帧类型、DLC 和硬件时间戳，打包为 `Can_Message_Struct` 写入环形缓冲区。

#### `HAL_FDCAN_RxFifo0Callback` / `HAL_FDCAN_RxFifo1Callback` *(HAL 回调)*
FIFO0 / FIFO1 收到新报文时触发，均调用 `CAN_Rx_Deal` 进行接收处理。

---

### 错误检测 / 协议分析

#### `CAN_ID_Frequence(id, dlc, if_extend)`
每收到一帧时调用。在 `IdStats[]` 表中查找该 ID，存在则帧计数加 1，不存在则新增记录（最多追踪 `MAX_TRACKED_IDS` 个）。同时估算本帧占用的物理比特数（标准帧 ≈ 44 + DLC×8，扩展帧 ≈ 68 + DLC×8，再叠加约 10% 位填充估算）累加到 `TotalBitsCount`。

#### `CAN_Statis_Task(uint32_t currentBaudRate)`
**1 秒调用一次。** 用累计比特数除以波特率得到总线利用率百分比，将各 ID 的 `FrameCount` 直接作为该 ID 在过去 1 秒内的帧频率（Hz），然后清零所有计数供下一秒重新统计。

#### `HAL_FDCAN_ErrorStatusCallback(hfdcan, ErrorStatusITs)` *(HAL 回调)*
协议层错误回调。读取 TEC/REC 计数器，判断并更新总线状态机（主动错误 / 被动错误 / 总线关闭）；识别并分类计数填充错误（Stuff）、格式错误（Form）、ACK 错误（ACK）、CRC 错误、位错误（Bit0/Bit1）。

#### `HAL_FDCAN_ErrorCallback(hfdcan)` *(HAL 回调)*
系统级故障回调。检测 FIFO0 / FIFO1 丢帧（`DroppedFrames++`）和 Message RAM 访问故障，并处理 HAL 库软件错误码。

---

## 附：各模块使用流程速查

```
UART:  My_UART_Init() → 主循环调 My_UART_LoopTask() → 检查 rx_notice/tx_notice

I2C:   My_I2C_Init() → 主机：ScanBus / Send / Read
                      → 从机：等待 AddrCallback 自动接收 → 检查 rx_notice

SPI:   My_SPI_Init() → 主机：My_SPI_Send / My_SPI_SendReceive
                     → 从机：主循环调 SPI_PutData_To_Buffer() → 读缓冲区

CAN:   HAL_FDCAN 初始化 → CAN_Handler_Start() → CAN_Filter_Config()
       → 主循环调 My_CAN_Send_Cycle() 和 CAN_Statis_Task()（1s一次）
       → 读 my_can_rangebuffer 取收到的报文
```
