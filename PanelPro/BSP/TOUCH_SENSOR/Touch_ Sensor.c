#include "TOUCH_SENSOR.h"

#define DEBOUNCE_CNT  3   // 连续 3 次确认

void TTP223_Init(TTP223_Button *btn, GPIO_TypeDef *port, uint16_t pin,
                 uint32_t long_press_ms, uint32_t repeat_ms,
                 TTP223_EventCallback callback)
{
    btn->port = port;
    btn->pin = pin;
    btn->long_press_ms = long_press_ms;
    btn->repeat_ms = repeat_ms;
    btn->callback = callback;

    btn->is_pressed = false;
    btn->long_triggered = false;
    btn->press_start_tick = 0;
    btn->last_repeat_tick = 0;
    btn->press_debounce = 0;
    btn->release_debounce = 0;
}

void TTP223_Scan(TTP223_Button *btn, uint32_t current_ms)
{
    // 读取当前电平（高 = 触摸，根据模块极性调整）
    bool level = (HAL_GPIO_ReadPin(btn->port, btn->pin) == GPIO_PIN_SET);

    //按下消抖
    if (level) {
        if (btn->press_debounce < DEBOUNCE_CNT) {
            btn->press_debounce++;
        }
        btn->release_debounce = 0;   // 有高电平，清零释放消抖
    } else {
        btn->press_debounce = 0;     // 低电平，清零按下消抖
    }

    // 释放消抖 
    if (!level) {
        if (btn->release_debounce < DEBOUNCE_CNT) {
            btn->release_debounce++;
        }
    } else {
        btn->release_debounce = 0;
    }

    // 状态机逻辑 
    if (!btn->is_pressed) {
        // 松开状态：按下消抖满  确认按下
        if (btn->press_debounce >= DEBOUNCE_CNT) {
            btn->is_pressed = true;
            btn->press_start_tick = current_ms;
            btn->last_repeat_tick = current_ms;
            btn->long_triggered = false;
        }
    } else {
        // 按下状态：释放消抖满 确认释放
        if (btn->release_debounce >= DEBOUNCE_CNT) {
            // 如果没有触发过长按 产生短按事件
            if (!btn->long_triggered) {
                if (btn->callback) {
                    btn->callback(btn, TTP223_EVENT_SHORT_PRESS);
                }
            }
            btn->is_pressed = false;
            btn->press_debounce = 0;
            btn->release_debounce = 0;
        } else {
            // 仍然处于按下，检查长按/重复
            if (level) {   // 只有在确实读到高电平时才计时，防止释放抖动期间误触发长按
                uint32_t elapsed = current_ms - btn->press_start_tick;
                if (elapsed >= btn->long_press_ms) {
                    if (!btn->long_triggered) {
                        btn->long_triggered = true;
                        btn->last_repeat_tick = current_ms;
                        if (btn->callback) {
                            btn->callback(btn, TTP223_EVENT_LONG_PRESS_START);
                        }
                    } else {
                        if ((current_ms - btn->last_repeat_tick) >= btn->repeat_ms) {
                            btn->last_repeat_tick = current_ms;
                            if (btn->callback) {
                                btn->callback(btn, TTP223_EVENT_LONG_PRESS_REPEAT);
                            }
                        }
                    }
                }
            }
        }
    }
}
