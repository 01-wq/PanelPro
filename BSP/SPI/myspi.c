#include "myspi.h"

// 信号量句柄定义
osSemaphoreId_t DMA_SemaphoreHandle = NULL;

/**
 * @brief 初始化 DMA 信号量
 */
void SPI_Init_Semaphore(void)
{
    DMA_SemaphoreHandle = osSemaphoreNew(1, 0, NULL);
    if (DMA_SemaphoreHandle == NULL) {
        /* Debug: DMA semaphore creation failed — see test_printf */
    }
}

/**
 * @brief 设置 SPI 为 16 位模式
 */
void SPI_Set_16Bit_Mode(void)
{
    __HAL_SPI_DISABLE(&hspi2);
    hspi2.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&hspi2);
    __HAL_SPI_ENABLE(&hspi2);
}

/**
 * @brief 恢复 SPI 为 8 位模式
 */
void SPI_Set_8Bit_Mode(void)
{
    __HAL_SPI_DISABLE(&hspi2);
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    HAL_SPI_Init(&hspi2);
    __HAL_SPI_ENABLE(&hspi2);
}

/**
 * @brief 使用 DMA 发送 16 位数据
 */
void SPI_Transmit_DMA_16Bit(uint16_t *pData, uint16_t Size)
{
    // 确保之前的 DMA 已完成
    if (HAL_DMA_GetState(&hdma_spi2_tx) != HAL_DMA_STATE_READY) {
        SPI_Wait_DMA_Complete();
    }
    
    // 切换到 16 位模式
    SPI_Set_16Bit_Mode();
    
    // 启动 DMA 传输
    HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)pData, Size * 2);
}

/**
 * @brief 等待 DMA 传输完成（阻塞，不占 CPU）
 */
void SPI_Wait_DMA_Complete(void)
{
    if (DMA_SemaphoreHandle != NULL) {
        // 阻塞等待信号量，超时 500ms
        osStatus_t status = osSemaphoreAcquire(DMA_SemaphoreHandle, 500);
        if (status != osOK) {
            /* Debug: DMA timeout — see test_printf */
        }
    } else {
        // 降级方案：有限次数轮询（防止完全卡死）
        uint32_t timeout = 10000;  // 最多 10000 次
        while (HAL_DMA_GetState(&hdma_spi2_tx) != HAL_DMA_STATE_READY && timeout--) {
            osDelay(1);  // 让出 CPU
        }
        if (timeout == 0) {
            /* Debug: DMA deadlock — see test_printf */
        }
    }
}
