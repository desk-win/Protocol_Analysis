# Memory Index

<!-- 此文件由 analyze-architecture skill 自动维护 -->

- [项目概览](project-overview.md) — STM32H747XIHx 双核, FreeRTOS, CMake/GCC 构建
- [双核架构](dual-core-arch.md) — CM7(显示+I2C) / CM4(摄像头), HSEM 同步, MPU 配置
- [外设分配](peripheral-map.md) — FMC SDRAM/LTDC/DMA2D/I2C2(CM7), DCMI/DMA(CM4)
- [模块依赖](module-deps.md) — #include 依赖链, 数据流, 模块分层(应用→HAL→驱动→中间件)
- [任务布局](task-layout.md) — CM7 defaultTask(LCD测试), CM4 defaultTask(空闲,预留给摄像头)
