#include "ring_buffer.h"

/**
 * @brief  初始化环形缓冲区 — 复位读写指针，清零数据区
 * @param  ring: 环形缓冲区指针
 * @retval None
 */
void RingBuffer_Init(RingBuffer_t *ring)
{
    ring->head = 0;   /* 读指针归零 */
    ring->tail = 0;   /* 写指针归零 */

    for (uint16_t i = 0; i < RING_SIZE; i++)
    {
        ring->buffer[i] = 0;
    }
}

/**
 * @brief  向环形缓冲区写入 1 字节 (ISR 上下文调用)
 * @param  ring: 环形缓冲区指针
 * @param  data: 待写入的字节
 * @retval 1 = 写入成功; 0 = 缓冲区已满
 */
uint8_t RingBuffer_Put(RingBuffer_t *ring, uint8_t data)
{
    /* 提前计算写入后的 tail 位置 */
    uint16_t next = (ring->tail + 1) % RING_SIZE;

    /* 满: 写指针 + 1 追上读指针 → 拒绝写入，丢字节 */
    if (next == ring->head) {
        return 0;
    }

    ring->buffer[ring->tail] = data;   /* 在 tail 处写入 */
    ring->tail = next;                  /* 推进写指针 */
    return 1;
}

/**
 * @brief  从环形缓冲区读出 1 字节 (主循环上下文调用)
 * @param  ring: 环形缓冲区指针
 * @param  data: 存放读出字节的地址
 * @retval 1 = 读出成功; 0 = 缓冲区空
 */
uint8_t RingBuffer_Get(RingBuffer_t *ring, uint8_t *data)
{
    /* 空: 读写指针重合 → 无数据可读 */
    if (ring->head == ring->tail) {
        return 0;
    }

    *data = ring->buffer[ring->head];                    /* 从 head 处读取 */
    ring->head = (ring->head + 1) % RING_SIZE;           /* 推进读指针 */
    return 1;
}

/**
 * @brief  查询缓冲区中可读的字节数
 * @param  ring: 环形缓冲区指针
 * @retval 可读字节数 (0 ~ RING_SIZE-1)
 */
uint16_t RingBuffer_Available(RingBuffer_t *ring)
{
    return (ring->tail - ring->head + RING_SIZE) % RING_SIZE;
}

/**
 * @brief  清空缓冲区 — 仅复位指针，不擦除数据
 * @param  ring: 环形缓冲区指针
 * @retval None
 */
void RingBuffer_Flush(RingBuffer_t *ring)
{
    ring->head = 0;
    ring->tail = 0;
}
