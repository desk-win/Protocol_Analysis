# 录制文件配置 header 设计

**日期**: 2026-06-25
**分支**: bare-lcd
**状态**: 设计已确认，待实施
**背景**: 现有录制系统(CM7 freertos.c)把波形字节写 SD `.log` 文件，文件名前缀标协议(d_uart/d_spi/...)，但**没存详细配置**(databits/parity/mode/datasize)。回放时波形按 live SHM_CONFIG 画，和录制当时可能不一致。

## 目标

录制时把当时**全部协议配置**(proto_config_t)写进文件 header；回放时读出来，按录制当时的配置画波形(framing/databits 正确还原)。向后兼容老文件(无 header)。

## 文件格式

```
offset 0:   uint32_t magic = 0x4C434552  ("RECL" LE)
offset 4:   proto_config_t cfg           (sizeof ~40B，含 active_proto + 4 协议全部字段)
offset 4+sizeof(cfg): 波形字节（和现在一样）
```

header 总长 `REC_HEADER_LEN = sizeof(magic) + sizeof(proto_config_t)`（CM7 编译器算，~44B）。

> 存全配置而非单协议：差 ~30B，相对 KB-MB 波形可忽略；代码最简单（不用 union/分支），active_proto 自带录制协议标识，未来扩展不愁。

## 改动点

### 1. `CM7/Core/Inc/shared_config.h`（+ CM4 同步，保持两核一致）
加：
```c
#define REC_MAGIC  0x4C434552U   /* "RECL" LE，录制文件 header 标识 */
```
（CM4 用不到，但两核 shared_config.h 保持字节一致是既有约定，避免 drift）

### 2. `CM7/Core/Src/main.c` — 加 playback 配置全局
```c
extern volatile uint32_t g_playback_header_len;   /* 当前回放文件 header 长度(0=老文件无 header) */
/* 上面是定义在 main.c 的全局，下面也加： */
volatile proto_config_t g_playback_cfg;           /* 回放读出的录制时配置（data_screen setParams 用）*/
volatile uint8_t  g_playback_cfg_valid = 0;       /* g_playback_cfg 是否有效(magic 匹配且读全) */
volatile uint32_t g_playback_header_len = 0;
```
> g_playback_cfg 不放 SHM_CONFIG（它是回放专用，不和 CM4 共享），就普通 CM7 全局。

### 3. `CM7/Core/Src/freertos.c` — 录制写 header
f_open 成功后（~line 362，sd_ready=1 之前），先写 header：
```c
/* 写配置 header：magic + 当时全部协议配置，回放时按此还原波形 framing */
SCB_InvalidateDCache_by_Addr((uint32_t*)SHM_CONFIG_ADDR, sizeof(proto_config_t) + 32);  /* 读最新 */
UINT bw = 0;
uint32_t magic = REC_MAGIC;
f_write(&sd_file, &magic, sizeof(magic), &bw);
f_write(&sd_file, (const void*)SHM_CONFIG, sizeof(proto_config_t), &bw);
f_sync(&sd_file);
```
之后波形字节追加在 header 后（现有 sd_buf 块写逻辑不动）。

### 4. `CM7/Core/Src/freertos.c` — 回放读 header + 偏移所有 seek
f_open 读（~line 293）成功后，先读 header：
```c
uint32_t magic = 0; UINT br = 0;
f_read(&play_file, &magic, sizeof(magic), &br);
if (br == sizeof(magic) && magic == REC_MAGIC) {
    UINT br2 = 0;
    f_read(&play_file, (void*)&g_playback_cfg, sizeof(proto_config_t), &br2);
    g_playback_cfg_valid = (br2 == sizeof(proto_config_t)) ? 1 : 0;
    g_playback_header_len = sizeof(magic) + sizeof(proto_config_t);
} else {
    f_lseek(&play_file, 0);              /* 老文件：从头当波形 */
    g_playback_cfg_valid = 0;
    g_playback_header_len = 0;
}
```
然后第一块波形读取（现 line 296 的 f_read）—— 文件位置已在 header 后，直接读即可。

**所有后续 seek/read 偏移 header_len**：
- reload 分支（~line 312）：`f_lseek(&play_file, g_playback_header_len + g_playback_pos);`（原来是 `g_playback_pos`）
- `g_playback_file_size = f_size(&play_file) - g_playback_header_len;`（波形净大小，进度条才准）

> g_playback_pos / buf_start 保持"波形相对偏移"语义（0-based 不含 header），UI 进度/拖动逻辑不用改。

### 5. `Data_screenView.{hpp,cpp}` — 回放按录制配置 setParams
- **hpp**：加 private 方法 `void applyWaveConfig(const proto_config_t* cfg);`（按 cfg->active_proto 算 databits/framed → waveWidget.setParams）
- **cpp**：把现有 setupScreen 里 setParams 那段（~line 51-64，按 active_proto 分支）抽成 `applyWaveConfig`，setupScreen 调 `applyWaveConfig(SHM_CONFIG)`
- **handleTickEvent**：检测 `g_playback_mode` 从 0→1（进回放），调 `applyWaveConfig(&g_playback_cfg)`（若 g_playback_cfg_valid；否则用 SHM_CONFIG 兜底）。这样回放时 widget 按录制配置画。
- 用一个 static `uint8_t prev_playback_mode` 在 handleTickEvent 里检测跳变。

## 向后兼容
- 老文件（无 magic）：header_len=0，从头读波形，配置用 live SHM_CONFIG（行为同现在，不回归）。
- 新文件在老固件上回放：header 字节会被当波形头几个字节显示（乱码几个字节），但能放。可接受（升级后建议重录）。

## 验证
1. 编译 CM7 通过
2. 录一段 UART（8N1），回放看 framing 是 8 数据位+无校验+1 停止位
3. 改 UART 配置成 7E1（应用），**回放刚才那段**，应仍是 8N1（按录制时配置，不受当前配置影响）← 核心验证
4. SPI 录制 datasize=4，回放画 4 个数据位
5. 老文件（升级前录的）能回放（header_len=0 兜底）

## 不做（YAGNI）
- 不存录制时间戳/注释（v2）
- 不做文件列表显示配置详情（v2）
- 不改文件名格式（仍 d_<proto>_<seq>.log）
