#ifndef __FLASH_OPS_H__
#define __FLASH_OPS_H__

#include <stdint.h>

uint8_t Flash_Erase_App_Area(void);
uint8_t MY_FLASH_Erase_Sector(uint32_t sector);
uint8_t MY_Flash_Write(uint32_t addr,const uint8_t *data,uint32_t len);
void Flash_Test(void);

#endif
