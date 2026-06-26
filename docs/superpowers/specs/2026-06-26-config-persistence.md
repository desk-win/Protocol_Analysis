# 配置持久化 设计

**日期**: 2026-06-26
**分支**: bare-lcd
**状态**: 设计已确认，待实施
**背景**: 现在 Apply 只写易失 SHM_CONFIG(SRAM1 @ 0x30001000),断电丢。要做成:Apply 时存 SD,上电读回,断电不丢。

## 目标

- Apply 时把当前 `proto_config_t` 写到 SD 的 `config.bin`
- 上电 CM7 读 `config.bin` → SHM_CONFIG → HSEM 通知 CM4 按保存的配置重配外设
- 文件缺失/损坏/版本不符 → 用代码默认值兜底(顺便治 SHM_CONFIG 上电随机值的老隐患)

## 文件格式

```
config.bin = [4B magic "CFG1"][proto_config_t 全部配置]
```
- `CFG_MAGIC = 0x31474643`("CFG1" LE)。版本号揉进 magic —— 以后 proto_config_t 结构改了(像这次加 role/cs_polarity),magic 升 "CFG2",旧文件自动判失效走 defaults,不用单独 version 字段。
- 文件大小 = 4 + sizeof(proto_config_t) ≈ 48B。

## 改动点

### 1. `shared_config.h`(两核同步)
- 加 `#define CFG_MAGIC 0x31474643U`("CFG1")
- 加 `static inline void shm_config_set_defaults(void)`：把 SHM_CONFIG 各字段填默认值(active_proto=1 UART + 各协议 *_DEFAULT_* 宏 + i2c.own_address=0x50)。两核都能调(CM7 boot 兜底用)。

### 2. `CM7/Core/Src/freertos.c` — 上电读 config.bin(SD ready 后,一次性)
在 SD mount 成功(`g_sd_status=4`)之后,加一次性引导读(用 static flag 防重复):
```c
static uint8_t cfg_loaded = 0;
if (g_sd_status == 4U && !cfg_loaded) {
    cfg_loaded = 1;
    FIL fcfg; UINT br = 0; uint32_t magic = 0;
    if (f_open(&fcfg, "0:config.bin", FA_READ) == FR_OK
        && f_read(&fcfg, &magic, 4, &br) == FR_OK && br == 4 && magic == CFG_MAGIC
        && f_read(&fcfg, (void*)SHM_CONFIG, sizeof(proto_config_t), &br) == FR_OK && br == sizeof(proto_config_t)
        && SHM_CONFIG->active_proto >= 1 && SHM_CONFIG->active_proto <= 4) {
        /* 读到有效配置，CM4 重配 */
    } else {
        /* 缺失/损坏/版本不符 → defaults + 写一份新的 */
        shm_config_set_defaults();
        write_config_bin();  // 见下小辅助
    }
    f_close(&fcfg);
    SCB_CleanDCache_by_Addr((uint32_t*)SHM_CONFIG_ADDR, sizeof(proto_config_t) + 32);
    shm_config_notify();   // HSEM_ID_1 → CM4 apply 读到的配置
}
```
> 关键：**复用现成 HSEM 配置 IPC**。CM7 读完 → `shm_config_notify()` → CM4 的 `HAL_HSEM_FreeCallback` → `g_config_pending` → 主循环 `apply_xxx_config_from_shm`。**CM4 零改动**。

### 3. `CM7 Settings_ScreenView.cpp` — Apply 时写 config.bin(覆盖)
`applyConfig()` 现有逻辑末尾(SHM_CONFIG 写完 + DCache clean + shm_config_notify 之后)，加：
```c
extern "C" void write_config_bin(void);  // freertos.c 或 main.c 提供，f_write config.bin
write_config_bin();   // 持久化当前 SHM_CONFIG 到 SD
```
但 SD 是 freertos defaultTask 专属访问者(FatFs 共享状态)，**不能从 UI 线程直接 f_write**(会和 defaultTask 的 sd_file 冲突)。所以用 flag：UI 设 `g_config_dirty=1`，defaultTask 看到 → f_write config.bin。

> 实现：main.c 加 `volatile uint8_t g_config_dirty = 0;`。applyConfig 末尾置 1。freertos defaultTask 加 `if (g_config_dirty) { g_config_dirty=0; write_config_bin(); }`(只在 !sd_ready 时,避免和录制冲突)。

### 4. `write_config_bin()` 辅助(CM7 freertos.c 或 main.c)
```c
void write_config_bin(void) {
    FIL fcfg;
    if (f_open(&fcfg, "0:config.bin", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
        UINT bw = 0; uint32_t magic = CFG_MAGIC;
        SCB_InvalidateDCache_by_Addr((uint32_t*)SHM_CONFIG_ADDR, sizeof(proto_config_t)+32); // 读最新
        f_write(&fcfg, &magic, 4, &bw);
        f_write(&fcfg, (const void*)SHM_CONFIG, sizeof(proto_config_t), &bw);
        f_close(&fcfg);
    }
}
```

## 时序
```
上电:
  CM7 main: 系统初始化 → Release HSEM_ID_0 → 唤醒 CM4
  CM4: HAL_Init + 外设默认 init(UART/I2C/SPI/CAN 默认值)→ 进裸循环(默认配置跑)
  CM7: osKernelStart → TouchGFX/defaultTask
  defaultTask: SD mount(g_sd_status=4)→ 读 config.bin → SHM_CONFIG → shm_config_notify
  CM4: 收 HSEM_ID_1 → apply_xxx → 重配成保存的配置  ← 秒级内,用户无感
```
> CM4 先用代码默认跑一下,被 CM7 读到的配置覆盖。中间 < 1 秒。

## 不做(YAGNI)
- 不做版本迁移(旧 magic 读不出直接 defaults,用户重设一次)
- 不做写次数限流(Apply 低频,wear-leveling 够)
- 不做配置多版本/回滚(只保留最新一份 config.bin)
- CM4 不读 SD(走 HSEM 让 CM7 单向推)

## 验证
1. 编译双核通过
2. Settings 改协议+参数 → Apply → 关机 → 重启 → **Settings 显示上次 Apply 的配置**(不是默认)
3. data_screen 波形协议 = 上次选的 active_proto
4. 首次开机(无 config.bin)→ defaults(UART 115200 8N1)→ 不崩,Settings 显示默认
5. 手动把 config.bin 改坏(magic 错)→ 启动走 defaults 不崩
