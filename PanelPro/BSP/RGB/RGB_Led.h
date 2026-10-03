#ifndef RGB_LED_H
#define RGB_LED_H

#include "stm32f4xx.h"                  // Device header


#define RGB_LED_PORT GPIOD
#define RGB_LED_PIN_R GPIO_PIN_8
#define RGB_LED_PIN_G GPIO_PIN_9
#define RGB_LED_PIN_B GPIO_PIN_10

// 使用条件编译避免重复定义
#ifndef RGB_COLOR_ENUM_DEFINED
#define RGB_COLOR_ENUM_DEFINED
enum{color_red,
     color_green,
     color_blue,
     color_purple,     
     color_yellow,
     color_cyan,
     color_white};
#endif
void RGB_LED_Init(void);
void change_color(uint8_t color);


#endif /* RGB_LED_H */
