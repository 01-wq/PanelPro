#ifndef __MYSPI_H
#define __MYSPI_H

#include "main.h"
#include "cmsis_os.h"

// 信号量句柄（外部可访问）
extern osSemaphoreId_t DMA_SemaphoreHandle;

// 函数声明
void SPI_Init_Semaphore(void);
void SPI_Set_16Bit_Mode(void);
void SPI_Set_8Bit_Mode(void);
void SPI_Transmit_DMA_16Bit(uint16_t *pData, uint16_t Size);
void SPI_Wait_DMA_Complete(void);

#endif /* __MYSPI_H */
