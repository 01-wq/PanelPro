#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx_hal.h"
#include "main.h"

#define KEY1 HAL_GPIO_ReadPin(KEY1_GPIO_Port,KEY1_Pin)

uint8_t KeyScan(uint8_t mode);

#endif /* __KEY_H */
