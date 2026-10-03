#ifndef __METADATA_H__
#define __METADATA_H__

#include <stdint.h>

#define APP_VALID_MAGIC 0xA5A5A5A5 //应用有效标志
#define METADATA_BASE   0x08008000

uint8_t Metadata_IsValid(void);
uint8_t Metadata_Invalidate(void);
void Metadata_Read(uint32_t *size,uint32_t *version);
uint8_t Metadata_Write(uint32_t size,uint32_t version);
void Metadata_test(void);

#endif
