#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"                  // Device header
#include <stdio.h>

#ifdef __cplusplus
extern "C" {s
#endif

typedef enum {
    TTP223_EVENT_NONE = 0,
    TTP223_EVENT_SHORT_PRESS,
    TTP223_EVENT_LONG_PRESS_START,
    TTP223_EVENT_LONG_PRESS_REPEAT,
} TTP223_Event;

typedef struct TTP223_Button TTP223_Button;

typedef void (*TTP223_EventCallback)(TTP223_Button *btn, TTP223_Event event);

struct TTP223_Button {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t long_press_ms;      // 长按阈值 (ms)
    uint32_t repeat_ms;          // 长按重复间隔 (ms)
    TTP223_EventCallback callback;

    // 内部变量
    bool     is_pressed;         // 当前是否处于按下状态
    bool     long_triggered;     // 本次按下是否已经触发过长按
    uint32_t press_start_tick;   // 按下的起始时间
    uint32_t last_repeat_tick;   // 上次重复触发的时间
	
	 // 消抖专用变量
    uint8_t  press_debounce;      // 按下消抖计数
    uint8_t  release_debounce;    // 释放消抖计数
};

void TTP223_Init(TTP223_Button *btn, GPIO_TypeDef *port, uint16_t pin,
                 uint32_t long_press_ms, uint32_t repeat_ms,
                 TTP223_EventCallback callback);

void TTP223_Scan(TTP223_Button *btn, uint32_t current_ms);

#ifdef __cplusplus
}
#endif
#endif

