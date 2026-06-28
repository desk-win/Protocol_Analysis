#ifndef __SHARED_CONFIG_H
#define __SHARED_CONFIG_H

#include <stdint.h>

/* 协议配置共享内存（CM7 UI 写 → CM4 读改外设 + CM7 读解读波形）
 * 放 SRAM1 的 0x30001000：ring buffer 在 0x30000000 占 ~2KB（shared_ring_t 到 0x30000804），
 * 留 ~2KB 间距到 0x30001000，防止 ring buffer 溢出污染 config。
 * SRAM1 MPU Region 4 non-cacheable，CM7 写直达物理，CM4（无 cache）读。*/
#define SHM_CONFIG_ADDR  0x30001000U

/* DCMI 结果缓冲区（双核共享，D2 SRAM1）
 *   adc_a_buf: ADC-A 通道解析结果，CHANNEL_SAMPLES 字节
 *   adc_b_buf: ADC-B 通道解析结果，CHANNEL_SAMPLES 字节
 *   dcmi_ctrl: 控制块（frame_ready + dropped）
 *
 *   CHANNEL_SAMPLES = SAMPLES_PER_LINE * LINES_PER_FRAME = 64 * 60 = 3840
 *
 *   地址紧挨 SHM_RING (2KB) + SHM_CONFIG (64B) + SHM_STATUS (32B) 之后 */
#define SHM_DCMI_ADDR       0x30002000U
#define SHM_DCMI_A_ADDR     (SHM_DCMI_ADDR)
#define SHM_DCMI_B_ADDR     (SHM_DCMI_ADDR + 3840U)
#define SHM_DCMI_CTRL_ADDR  (SHM_DCMI_B_ADDR + 3840U)

#define DCMI_ADC_A   ((volatile uint8_t *)SHM_DCMI_A_ADDR)
#define DCMI_ADC_B   ((volatile uint8_t *)SHM_DCMI_B_ADDR)

typedef struct {
    volatile uint8_t  frame_ready;   // 1 = 有新帧，CM7 读完清 0
    volatile uint16_t frame_id;      // 帧序号，CM7 据此判断是否新帧
    volatile uint32_t dropped;       // M4 丢弃计数
} dcmi_shm_ctrl_t;

#define DCMI_SHM_CTRL  ((volatile dcmi_shm_ctrl_t *)SHM_DCMI_CTRL_ADDR)

/* DMA_Catch 波形缓冲区（双核共享，D2 SRAM1）
 *   TIM1 触发 DMA1 以 10MHz 从 GPIOG->IDR 快照 uint16_t。
 *   双缓冲：前半 10000 条 + 后半 10000 条 = 20000 × uint16_t = 40 KB
 *   DCMI 区域结束于 0x30003E10，DMA_Catch 从 0x30004000 开始 */
#define SHM_DMA_CATCH_ADDR      0x30004000U
#define SHM_DMA_CATCH_HALF      10000U
#define SHM_DMA_CATCH_SIZE      (SHM_DMA_CATCH_HALF * 2U)   /* 20000 条 */

#define DMA_CATCH_BUF  ((volatile uint16_t *)SHM_DMA_CATCH_ADDR)

/* 控制块紧接缓冲区之后 */
#define SHM_DMA_CATCH_CTRL_ADDR (SHM_DMA_CATCH_ADDR + (SHM_DMA_CATCH_SIZE * sizeof(uint16_t)))

typedef struct {
    volatile uint8_t  batch_ready;   /* bit0=前半就绪, bit1=后半就绪，CM7 读完清对应位 */
    volatile uint32_t sample_count;  /* 累计采样总数（CM7 据此判断有无新数据、算时间轴）*/
} dma_catch_ctrl_t;

#define DMA_CATCH_CTRL  ((volatile dma_catch_ctrl_t *)SHM_DMA_CATCH_CTRL_ADDR)

/* HSEM 通知 ID（CM7 写完 config 后 Release → CM4 HSEM 中断读 config 重配外设）
 * HSEM_ID_0（=0）被双核 boot 同步占用（main.c CM7 Release 唤醒 CM4），用 ID=1。
 * FreeRTOSConfig.h 没占用任何 HSEM ID，ID=1 空闲。 */
#define HSEM_ID_CONFIG  1U
#define HSEM_ID_DONE    2U
/* ====== UART 配置 ====== */
typedef struct {
    uint32_t baudrate;     /* 实际波特率：9600/19200/38400/57600/115200/230400/460800/921600 */
    uint8_t  databits;     /* 5/6/7/8/9 */
    uint8_t  stopbits;     /* 1, 2, 3(=1.5) */
    uint8_t  parity;       /* 0=None, 1=Even, 2=Odd */
    uint8_t  flowcontrol;  /* 0=None, 1=RTS, 2=CTS, 3=RTS_CTS */
} uart_config_t;

/* ====== SPI 配置 ====== */
typedef struct {
    uint8_t  role;         /* 0=Slave(默认), 1=Master */
    uint8_t  cs_polarity;  /* 0=Active-Low(默认), 1=Active-High（v1 不进 UI，硬编码 0）*/
    uint8_t  mode;         /* 0-3: CPOL/CPHA 组合（0=00, 1=01, 2=10, 3=11）*/
    uint8_t  datasize;     /* 4-8 bit（v1 限制：BDMA byte 对齐）*/
    uint32_t baudrate;     /* slave 无效；master 见 spi_prescaler_from_baud */
    uint8_t  firstbit;     /* 0=MSB first, 1=LSB first */
} spi_config_t;

/* ====== I2C 配置 ====== */
typedef struct {
    uint8_t own_mode;      /* 配置主从模式，0=从，1=主 */
    uint32_t clock_speed;  /* 100000(标准)/400000(快速)/1000000(快速+) */
    uint8_t  addressing;   /* 0=7-bit, 1=10-bit */
    uint16_t own_address;  /* 自身地址 */
} i2c_config_t;

/* ====== CAN 配置 ====== */
typedef struct {
    uint32_t baudrate;     /* 50000/125000/250000/500000/1000000 */
    uint8_t  mode;         /* 0=Normal, 1=Loopback, 2=Silent, 3=Loopback+Silent */

    /* ── 发送报文格式 ── */
    uint32_t tx_id;        /* 发送的目标 CAN ID */
    uint8_t  tx_id_type;   /* 0=标准帧(11bit), 1=扩展帧(29bit) */
    uint8_t  tx_frame_type;/* 0=数据帧, 1=遥控帧 */
    uint8_t  tx_dlc;       /* 数据长度 0~8 字节 */

    /* ── 接收过滤器 ── */
    uint8_t  filter_mode;   /* 0=全通（收所有帧）, 1=精准 ID 过滤 */
    uint32_t filter_id;     /* 过滤模式下要接收的目标 ID */
    uint8_t  filter_id_type;/* 0=标准帧, 1=扩展帧 */
    uint8_t  filter_fifo;   /* 0=FIFO0, 1=FIFO1 */
} can_config_t;

/* 协议配置（嵌套所有协议，active_proto 指示当前选中）*/
typedef struct {
    uint8_t active_proto;  /* 0=none, 1=UART, 2=SPI, 3=I2C, 4=CAN */
    uart_config_t uart;
    spi_config_t  spi;
    i2c_config_t  i2c;
    can_config_t  can;
    uint8_t dcmi_enable;
    uint8_t dma_catch_enable;
} proto_config_t;

/* 用作M4通知M7的标志位 */
#define SHM_STATUS_ADDR  (SHM_CONFIG_ADDR + 64U)   // 留 64 字节给 proto_config_t 够用
typedef struct {
    volatile uint8_t protocol_done;   // M4 置 1：当前协议通信完成
    volatile uint8_t protocol_error;  // M4 置：0=正常，非 0=错误码
    volatile uint32_t reserved[3];    // 对齐
} shm_status_t;

#define SHM_STATUS  ((volatile shm_status_t *)SHM_STATUS_ADDR)

/* M7→M4 发送数据缓冲区（主机模式下，M7 UI 填入要发送的数据）
 *   紧挨 SHM_STATUS (0x30001060)，单次最大 256 字节，四个协议共用 */
#define SHM_TX_BUF_ADDR   (SHM_STATUS_ADDR + 32U)    /* 0x30001060 */
#define SHM_TX_BUF_SIZE   256U

#define SHM_TX_BUF   ((uint8_t *)SHM_TX_BUF_ADDR)
#define SHM_TX_LEN   ((volatile uint16_t *)(SHM_TX_BUF_ADDR + SHM_TX_BUF_SIZE))
/* M7 写 SHM_TX_LEN = 要发送的字节数 → M4 协议 Task 读取发送 → M4 发送完清 0 */

#define SHM_CONFIG  ((proto_config_t *)SHM_CONFIG_ADDR)

/* 录制文件 header 标识（"RECL" LE）：.log 文件首 4 字节，后跟 proto_config_t 再跟波形字节 */
#define REC_MAGIC         0x4C434552U
#define REC_HEADER_LEN    (sizeof(uint32_t) + sizeof(proto_config_t))

/* 默认值（CM4 上电初始化 / UI 默认）*/
#define UART_DEFAULT_BAUDRATE   115200U
#define UART_DEFAULT_DATABITS   8U
#define UART_DEFAULT_STOPBITS   1U
#define UART_DEFAULT_PARITY     0U
#define UART_DEFAULT_FLOWCTRL   0U

#define SPI_DEFAULT_MODE        0U
#define SPI_DEFAULT_DATASIZE    8U
#define SPI_DEFAULT_BAUDRATE    1000000U
#define SPI_DEFAULT_FIRSTBIT    0U
#define SPI_DEFAULT_ROLE        0U
#define SPI_DEFAULT_CS_POL      0U

#define I2C_DEFAULT_MODE        0U      /* 0=从机 */
#define I2C_DEFAULT_CLOCK       100000U
#define I2C_DEFAULT_ADDRMODE    0U      /* 0=7-bit */
#define I2C_DEFAULT_OWNADDR     0x50U

#define CAN_DEFAULT_BAUDRATE    500000U
#define CAN_DEFAULT_MODE        0U
#define CAN_DEFAULT_TX_ID       0x123U
#define CAN_DEFAULT_TX_ID_TYPE  0U      /* 0=标准帧 */
#define CAN_DEFAULT_TX_FRAME_TYPE 0U    /* 0=数据帧 */
#define CAN_DEFAULT_TX_DLC      8U
#define CAN_DEFAULT_FILTER_MODE  0U     /* 0=全通 */
#define CAN_DEFAULT_FILTER_ID    0U
#define CAN_DEFAULT_FILTER_ID_TYPE 0U
#define CAN_DEFAULT_FILTER_FIFO  0U     /* 0=FIFO0 */

/* 配置持久化 config.bin = magic + proto_config_t。
 * Apply 时覆盖写，上电 CM7 读 → HSEM 通知 CM4 按保存的配置重配。*/
#define CFG_MAGIC  0x31474643U   /* "CFG1" LE；proto_config_t 结构改了升 "CFG2"，旧文件自动判失效 */

/* SHM_CONFIG 填代码默认值（config.bin 缺失/损坏/版本不符时兜底；两核可调，CM7 boot 用）*/
static inline void shm_config_set_defaults(void)
{
    SHM_CONFIG->active_proto        = 1U;  /* UART */
    SHM_CONFIG->uart.baudrate       = UART_DEFAULT_BAUDRATE;
    SHM_CONFIG->uart.databits       = UART_DEFAULT_DATABITS;
    SHM_CONFIG->uart.stopbits       = UART_DEFAULT_STOPBITS;
    SHM_CONFIG->uart.parity         = UART_DEFAULT_PARITY;
    SHM_CONFIG->uart.flowcontrol    = UART_DEFAULT_FLOWCTRL;
    SHM_CONFIG->spi.role            = SPI_DEFAULT_ROLE;
    SHM_CONFIG->spi.cs_polarity     = SPI_DEFAULT_CS_POL;
    SHM_CONFIG->spi.mode            = SPI_DEFAULT_MODE;
    SHM_CONFIG->spi.datasize        = SPI_DEFAULT_DATASIZE;
    SHM_CONFIG->spi.baudrate        = SPI_DEFAULT_BAUDRATE;
    SHM_CONFIG->spi.firstbit        = SPI_DEFAULT_FIRSTBIT;
    SHM_CONFIG->i2c.own_mode        = I2C_DEFAULT_MODE;
    SHM_CONFIG->i2c.clock_speed     = I2C_DEFAULT_CLOCK;
    SHM_CONFIG->i2c.addressing      = I2C_DEFAULT_ADDRMODE;
    SHM_CONFIG->i2c.own_address     = I2C_DEFAULT_OWNADDR;
    SHM_CONFIG->can.baudrate        = CAN_DEFAULT_BAUDRATE;
    SHM_CONFIG->can.mode            = CAN_DEFAULT_MODE;
    SHM_CONFIG->can.tx_id           = CAN_DEFAULT_TX_ID;
    SHM_CONFIG->can.tx_id_type      = CAN_DEFAULT_TX_ID_TYPE;
    SHM_CONFIG->can.tx_frame_type   = CAN_DEFAULT_TX_FRAME_TYPE;
    SHM_CONFIG->can.tx_dlc          = CAN_DEFAULT_TX_DLC;
    SHM_CONFIG->can.filter_mode     = CAN_DEFAULT_FILTER_MODE;
    SHM_CONFIG->can.filter_id       = CAN_DEFAULT_FILTER_ID;
    SHM_CONFIG->can.filter_id_type  = CAN_DEFAULT_FILTER_ID_TYPE;
    SHM_CONFIG->can.filter_fifo     = CAN_DEFAULT_FILTER_FIFO;
    SHM_CONFIG->dcmi_enable         = 0U;
    SHM_CONFIG->dma_catch_enable    = 0U;
}

#endif /* __SHARED_CONFIG_H */
