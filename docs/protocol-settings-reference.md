# 协议设置参数对照表（Settings 屏 → 硬件）

每个协议在 Settings 屏有若干参数行，每个值都直接映射到外设寄存器的一个字段。**要和被测设备对齐才能通信。** 本文档逐个说清楚"值 → 含义 → 硬件 → 怎么选"，重点标出易混淆项。

> 数据来源：UI 档位表 [Settings_ScreenView.cpp:14-31](../CM7/TouchGFX/gui/src/settings_screen_screen/Settings_ScreenView.cpp#L14-L31)，硬件映射 [CM4/Core/Src/main.c](../CM4/Core/Src/main.c) 的 `apply_xxx_config_from_shm`。

---

## UART（5 行参数）

| UI 行 | 选项 | 含义 | 硬件字段 |
|---|---|---|---|
| **Baud** | 9600 / 19200 / 38400 / 57600 / 115200 / 230400 / 460800 / 921600 | 波特率 bits/s | `huart6.Init.BaudRate` |
| **Data** | 7 / 8 / 9 | 数据位 | `huart6.Init.WordLength`（见下方易混淆⚠️）|
| **Stop** | 1 / 2 | 停止位 | `huart6.Init.StopBits` |
| **Parity** | None / Even / Odd | 校验位 | `huart6.Init.Parity` |
| **Flow** | None / RTS / CTS / RTS-CTS | 硬件流控 | `huart6.Init.HwFlowCtI` |

### ⚠️ 易混淆：Data + Parity 的关系
STM32 USART 的 `WordLength` **含校验位**。所以：
- Data=8 + Parity=Even → 硬件实际是 **WordLength=9B**（8 数据 + 1 校验），帧长 10 bit（含 start/stop）
- Data=8 + Parity=None → WordLength=8B（纯 8 数据）
- 想要"7 数据位 + Even 校验"→ 选 Data=7 + Parity=Even，硬件升档成 8B（7 数据 + 1 校验）

apply 端已自动处理升档，UI 选什么数据位就是**纯数据位**，不用自己算校验位。但要知道**总帧长会因 parity 多 1 bit**。

### 限制
- Data 只 7/8/9（5/6 退化为 8B，硬件不支持）
- Stop 只 1/2（1.5 仅智能卡模式，退化为 2）

### 怎么选
照被测 UART 设备的波特率/数据位/校验/停止位填。流控一般 None（除非设备用了 RTS/CTS 线）。

---

## SPI（5 行参数）

| UI 行 | 选项 | 含义 | 硬件字段 | 何时生效 |
|---|---|---|---|---|
| **Mode** | 0 / 1 / 2 / 3 | CPOL+CPHA 组合（见下表⚠️）| `hspi6.Init.CLKPolarity`/`CLKPhase` | slave+master |
| **Data** | 4 / 5 / 6 / 7 / 8 | 每帧数据位 | `hspi6.Init.DataSize` | slave+master |
| **Baud** | 187500 / 375000 / 750000 / 1500000 / 3000000 / 6000000 | SCK 频率 Hz | `hspi6.Init.BaudRatePrescaler` | **仅 Master** |
| **First** | MSB / LSB | 位序（高位先/低位先）| `hspi6.Init.FirstBit` | slave+master |
| **Role** | Slave / Master | 我们的板子当从机/主机 | `My_SPI_Init(MASTER/SLAVE)` | — |

### ⚠️ 易混淆：Mode 0~3（CPOL/CPHA）
这是 SPI 最容易错的点，**必须和被测设备 datasheet 一致**。

| Mode | CPOL（SCK 空闲电平）| CPHA（采样沿）| 常见度 |
|---|---|---|---|
| **0** | 低 | 第一个沿（上升沿采样）| ⭐ 最常见，90% 设备 |
| **1** | 低 | 第二个沿（下降沿采样）| 少 |
| **2** | 高 | 第一个沿（下降沿采样）| 少 |
| **3** | 高 | 第二个沿（上升沿采样）| ⭐ 第二常见 |

记法：CPOL=空闲电平，CPHA=0 在第一个沿采样、CPHA=1 在第二个沿采样。

### ⚠️ Baud 标签待验证
UI 数字按"SPI6 时钟=12MHz"算，但 .ioc 标 SPI6 时钟=120MHz。**实际波特率可能 ×10**（选 187500 实际可能 1.875MHz）。Slave 模式忽略此值（跟主机时钟）。**首次上电用逻辑分析仪量 PG13 确认**，错了改 UI 标签（prescaler 索引逻辑是对的）。

### 不在 UI 但需知道
| 项 | 当前值 | 说明 |
|---|---|---|
| **CS 极性** | 硬编码 **Active-Low**（低有效）| UI 5 行塞不下，v1 没暴露。绝大多数 SPI 设备低有效，默认够用 |
| **CS→SCK 延时** | 0 | CS 拉低到首沿建立时间，0=无延时 |

### 限制
- Data 只到 8（12/16 已砍掉 —— BDMA byte 对齐不支持，见 spec §8）

### 怎么选
- **测别人的 SPI master**：Role=Slave，Mode/Data/First 照那个 master 填，Baud 忽略
- **测 SPI 从机设备/自环回**：Role=Master，照从机 datasheet 填全部。⚠️ v1 master 只发递增字节，不能发指定命令读寄存器（v2）

---

## I2C（2 行参数 + 1 个隐藏项⚠️）

| UI 行 | 选项 | 含义 | 硬件字段 |
|---|---|---|---|
| **Clock** | 100000 / 400000 / 1000000 | SCL 频率（标准 100k / 快速 400k / 快速+ 1M）| `hi2c4.Init.Timing`（My_I2C_Init 算）|
| **Addr** | 7-bit / 10-bit | 寻址位宽 | `hi2c4.Init.AddressingMode` |

### ⚠️ 隐藏项：own_address（从机地址）改不了
我们的板子当 **I2C 从机**（被动收外部 master 的数据）。从机地址（own_address）**UI 没暴露**，applyConfig 不写这个字段，一直是默认 **0x50**。

代码：CM4 main.c:159 `My_I2C_Init(MY_I2C_SLAVE, 7, 0x50, ...)` 默认 0x50；apply_i2c 读 `i->own_address` 但 UI 从不设置它。

**后果**：要测一个发到 0x50 的 master 没问题；要测发到别的地址的 master，**得改代码**（main.c:159 的 0x50 或在 shared_config.h 加默认值，再在 UI 加栏）。v2 可补。

### 怎么选
- Clock 照被测 master 的 SCL 频率填（一般 100k 或 400k）
- Addr 照 master 的寻址位宽填（7-bit 占多数）
- **确认外部 master 是发到 0x50 的**，否则收不到（见上⚠️）

---

## CAN（2 行参数）

| UI 行 | 选项 | 含义 | 硬件字段 |
|---|---|---|---|
| **Baud** | 50000 / 125000 / 250000 / 500000 / 1000000 | 波特率 bits/s | BRP 算（`CAN_Param_Change`）|
| **Mode** | Normal / Loopback / Silent / LB+Silent | 工作模式（见下⚠️）| `hfdcan1.Init.Mode` |

### ⚠️ 易混淆：Mode 4 种模式
| Mode | 含义 | 用途 |
|---|---|---|
| **Normal** | 正常收发（发帧带 ACK，影响总线）| 接入真实 CAN 总线 |
| **Loopback** | 外部回环（TX→总线→RX，帧上总线）| 不接外部收发器自测，但帧会出现在 TX/RX 引脚 |
| **Silent** | 只听不发（监听总线，不 ACK 不发）| 当分析仪嗅探总线，不影响总线 |
| **LB+Silent** | 内部回环（IP 内部 TX→RX，**不上总线**）| ⭐ 完全自测推荐，不干扰任何总线 |

代码映射（apply_can）：0=Normal, 1=EXTERNAL_LOOPBACK, 2=BUS_MONITORING(Silent), 3=INTERNAL_LOOPBACK(LB+Silent)。

### 波特率算法（已验证）
FDCAN kernel clock = 240MHz（PLL1Q）。固定 BS1=13 / BS2=2 / SJW=1（16 tq，采样点 87.5%），`BRP = 240M / (Baud × 16)`。例：500k → BRP=30。

### 怎么选
- 接真实 CAN 总线：Mode=Normal，Baud 照总线填
- 纯嗅探不干扰：Mode=Silent
- 自测通路：Mode=LB+Silent（不用接 CAN 收发器）

---

## 共通注意事项

1. **active_proto 决定哪条通路跑**：UI 行0 选协议 → `SHM_CONFIG->active_proto`（1=UART/2=SPI/3=I2C/4=CAN）→ CM4 HSEM 中断 → 重配对应外设 + main 循环走对应数据通路。其他三个协议的硬件不会停（但 main 循环不读它们的数据）。
2. **改参数后要点 Apply**：Apply 才写 SHM_CONFIG + DCache clean + HSEM 通知 CM4。Discard 不写（回退 snap）。
3. **切协议会弹窗**：行0 +/- 切协议时，若当前协议有未 Apply 的改动，弹 Apply/Discard/Cancel 确认。
4. **隐藏限制（跨协议）**：
   - SPI Data>8、I2C own_address、CS 极性 → UI 都没暴露（v2 待补）
   - 协议分析统计（CS 脉宽/帧间隔/成功率）→ 没传到屏幕显示（v2）

---

## 关联
- SPI 详细设计：[specs/2026-06-25-spi-integration-design.md](superpowers/specs/2026-06-25-spi-integration-design.md)
- 协议移植状态：记忆 `can-migration.md`
