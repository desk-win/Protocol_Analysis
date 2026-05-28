---
name: analyze-architecture
description: 扫描项目整体代码框架，分析模块职责和依赖关系，并将结果写入长期记忆。每次代码结构变更后调用即可更新记忆。
---

# 代码框架分析与长期记忆

当用户调用此 Skill 时，执行以下步骤：

## 第1步：项目全景扫描

1. 列出项目顶层目录结构（`ls -d */`）
2. 读取 `CMakeLists.txt` 理解构建目标和源文件组织
3. 读取 `qiansai.ioc`（STM32CubeMX 工程文件）获取外设和中间件配置概况
4. 读取 `CM4/` 和 `CM7/` 下的目录结构，理解双核代码划分
5. 列出 `Drivers/` 和 `Middlewares/` 的目录树（浅层）

## 第2步：核心模块分析

1. 搜索 `main.c` 或 `main.cpp`，阅读主函数，梳理初始化流程和主循环
2. 搜索所有 `*_Init()` 函数调用，整理外设初始化链路
3. 搜索中断服务函数 `IRQHandler` 和中断向量表配置
4. 查找核间通信机制：HSEM、IPC、共享内存相关代码
5. 查找关键外设驱动：FMC、LTDC、DMA2D、SDRAM 等
6. 搜索 `freertos.c` 或 RTOS 相关配置，了解任务划分
7. 如果存在 `app_*.c` 或 `*_task.c`，阅读其职责

## 第3步：依赖关系梳理

1. 搜索 `#include` 关系，特别是跨核、跨模块的引用
2. 识别数据流向：中断 → 缓冲区 → 处理任务 → 输出 等链路
3. 列出共享的 `Common/` 模块和各自私有的 `Core/` 代码

## 第4步：写入长期记忆

将分析结果写入 `.claude/memory/` 目录，每项一个文件，使用以下 frontmatter 格式：

```markdown
---
name: {{短横线命名的slug}}
description: {{一句话概述}}
metadata:
  type: project
---

{{具体内容}}
```

**至少创建以下记忆文件：**

1. `project-overview.md` — 项目概要：芯片型号、双核架构、RTOS、构建系统
2. `dual-core-arch.md` — 双核架构：CM4/CM7 各自职责、核间通信方式、共享资源
3. `peripheral-map.md` — 外设分配表：每个外设由哪个核管理，初始化代码位置
4. `module-deps.md` — 模块依赖关系：关键模块及 #include 依赖链
5. `task-layout.md` — RTOS 任务布局（如果有 FreeRTOS）：任务名称、优先级、职责

最后更新 `.claude/memory/MEMORY.md` 索引文件，格式：

```markdown
- [项目概览](project-overview.md) — 芯片型号、架构简述
- [双核架构](dual-core-arch.md) — CM4/CM7 职责划分
- [外设分配](peripheral-map.md) — 外设与核的对应关系
- [模块依赖](module-deps.md) — 关键模块的 #include 依赖链
- [任务布局](task-layout.md) — RTOS 任务划分
```

## 第5步：总结汇报

向用户报告分析结果摘要（不超过20行），列出：
- 发现的核心数量（单核/双核）
- 关键外设和中间件
- RTOS 情况
- 写入的记忆文件列表

## 增量更新

如果用户说"更新架构分析"或代码有较大变更：
1. 读取已有记忆文件了解上次分析结论
2. 只扫描变更涉及的文件（通过 git diff）
3. 更新对应的记忆文件
4. 如果结论不变就保持不变，不重复写入