#ifndef __SHARED_BUF_H
#define __SHARED_BUF_H

#include <stdint.h>
#include "string.h"
/* CM4↔CM7 共享内存环形缓冲（SRAM4 / D3 domain 0x38000000，双核共享）
 * MPU 已把 SRAM4 配为 shareable + non-cacheable，单生产者(CM4)单消费者(CM7)
 * 的无锁环形缓冲，不需要 HSEM。 */
#define SHM_BUF_ADDR    0x30000000U   /* SRAM1 (D2域), CM4直接访问, CM7跨域访问 */
#define SHM_BUF_SIZE    2048U   /* 2KB 字节缓冲 */

typedef struct {
    volatile uint16_t head;     /* CM4 写位置（仅 CM4 推进） */
    volatile uint16_t tail;     /* CM7 读位置（仅 CM7 推进） */
    uint8_t data[SHM_BUF_SIZE]; /* 字节数据 */
} shared_ring_t;

#define SHM_RING  ((shared_ring_t *)SHM_BUF_ADDR)

/* 清空缓冲（由 CM7 启动时调用一次，CM7 先于 CM4 跑） */
static inline void shm_init(void) {
    SHM_RING->head = 0;
    SHM_RING->tail = 0;
}

/* CM4 写一个字节（满则覆盖最老的，保证最新数据不丢）。
 * 注意：tail 由 CM4 写（覆盖时推进），CM4→CM7 方向通，CM7 能读到最新 tail，
 * 从而知道哪些数据已被覆盖。波形读取用 head 往回数 N 字节，天然配合循环覆盖。*/
static inline void shm_push(uint8_t b) {
    uint16_t next = (uint16_t)((SHM_RING->head + 1U) % SHM_BUF_SIZE);
    if (next == SHM_RING->tail) {
        /* 满了，覆盖最老数据（推进 tail）*/
        SHM_RING->tail = (uint16_t)((SHM_RING->tail + 1U) % SHM_BUF_SIZE);
    }
    SHM_RING->data[SHM_RING->head] = b;
    SHM_RING->head = next;
}

/* CM7 读一个字节，返回 1=读到 0=空 */
static inline int shm_pop(uint8_t *b) {
    if (SHM_RING->head == SHM_RING->tail) return 0;
    *b = SHM_RING->data[SHM_RING->tail];
    SHM_RING->tail = (uint16_t)((SHM_RING->tail + 1U) % SHM_BUF_SIZE);
    return 1;
}

/* CM7 查询缓冲里还有多少字节没读 */
static inline uint16_t shm_count(void) {
    int16_t n = (int16_t)SHM_RING->head - (int16_t)SHM_RING->tail;
    return (uint16_t)((n >= 0) ? n : n + SHM_BUF_SIZE);
}

/* 多字节写入 */
static inline void shm_push_u16(uint16_t val) {
    shm_push((uint8_t)(val));
    shm_push((uint8_t)(val >> 8));
}

static inline void shm_push_u32(uint32_t val) {
    shm_push((uint8_t)(val));
    shm_push((uint8_t)(val >> 8));
    shm_push((uint8_t)(val >> 16));
    shm_push((uint8_t)(val >> 24));
}

static inline void shm_push_float(float val) {
    uint32_t raw;
    memcpy(&raw, &val, 4);              // 把 float 的 4 字节原样复制到 uint32_t
    shm_push_u32(raw);              // 再推出去
}

/* 写一整个字节数组到共享环 */
static inline void shm_push_buf(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        shm_push(data[i]);
    }
}

#endif /* __SHARED_BUF_H */
