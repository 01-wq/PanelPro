#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdint.h>

#define RING_SIZE 512

/**
 * @brief 环形缓冲区 — 中断与主循环之间的数据桥梁
 *
 * 惯例:
 *   head = 读指针 (主循环通过 RingBuffer_Get 消费数据)
 *   tail = 写指针 (ISR 通过 RingBuffer_Put 生产数据)
 *
 * 空: head == tail
 * 满: (tail + 1) % RING_SIZE == head  (故意浪费 1 字节以区分空/满)
 */
typedef struct
{
    uint8_t  buffer[RING_SIZE];
    volatile uint16_t head;   /* 读指针 — 主循环修改 */
    volatile uint16_t tail;   /* 写指针 — ISR 修改 */
} RingBuffer_t;

void     RingBuffer_Init(RingBuffer_t *ring);
uint8_t  RingBuffer_Put(RingBuffer_t *ring, uint8_t data);
uint8_t  RingBuffer_Get(RingBuffer_t *ring, uint8_t *data);
uint16_t RingBuffer_Available(RingBuffer_t *ring);
void     RingBuffer_Flush(RingBuffer_t *ring);

#endif /* RING_BUFFER_H_ */
