#ifndef __SHARED_CONFIG_H
#define __SHARED_CONFIG_H

#include <stdint.h>

/* 协议配置共享内存（CM7 UI 写 → CM4 读改外设 + CM7 读解读波形）
 * 放 SRAM1 的 0x30001000：ring buffer 在 0x30000000 占 ~2KB（shared_ring_t 到 0x30000804），
 * 留 ~2KB 间距到 0x30001000，防止 ring buffer 溢出污染 config。
 * SRAM1 MPU Region 4 non-cacheable，CM7 写直达物理，CM4（无 cache）读。*/
#define SHM_CONFIG_ADDR  0x30001000U

/* HSEM 通知 ID（CM7 写完 config 后 Release → CM4 HSEM 中断读 config 重配外设）
 * HSEM_ID_0（=0）被双核 boot 同步占用（main.c CM7 Release 唤醒 CM4），用 ID=1。
 * FreeRTOSConfig.h 没占用任何 HSEM ID，ID=1 空闲。 */
#define HSEM_ID_CONFIG  1U

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
    uint8_t  mode;         /* 0-3: CPOL/CPHA 组合（0=00, 1=01, 2=10, 3=11）*/
    uint8_t  datasize;     /* 4-16 bit */
    uint32_t baudrate;     /* 实际波特率（CM4 算 prescaler）*/
    uint8_t  firstbit;     /* 0=MSB first, 1=LSB first */
} spi_config_t;

/* ====== I2C 配置 ====== */
typedef struct {
    uint32_t clock_speed;  /* 100000(标准)/400000(快速)/1000000(快速+) */
    uint8_t  addressing;   /* 0=7-bit, 1=10-bit */
    uint16_t own_address;  /* 自身地址 */
} i2c_config_t;

/* ====== CAN 配置 ====== */
typedef struct {
    uint32_t baudrate;     /* 50000/125000/250000/500000/1000000 */
    uint8_t  mode;         /* 0=Normal, 1=Loopback, 2=Silent, 3=Loopback+Silent */
} can_config_t;

/* 协议配置（嵌套所有协议，active_proto 指示当前选中）*/
typedef struct {
    uint8_t active_proto;  /* 0=none, 1=UART, 2=SPI, 3=I2C, 4=CAN */
    uart_config_t uart;
    spi_config_t  spi;
    i2c_config_t  i2c;
    can_config_t  can;
} proto_config_t;

#define SHM_CONFIG  ((proto_config_t *)SHM_CONFIG_ADDR)

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

#define I2C_DEFAULT_CLOCK       100000U
#define I2C_DEFAULT_ADDRMODE    0U
#define I2C_DEFAULT_OWNADDR     0x50U

#define CAN_DEFAULT_BAUDRATE    500000U
#define CAN_DEFAULT_MODE        0U

#endif /* __SHARED_CONFIG_H */
