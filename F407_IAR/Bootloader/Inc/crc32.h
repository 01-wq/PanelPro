#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

void CRC32_Init(void);
uint32_t CRC32_Calc(const uint8_t *data,uint32_t len);

#endif
