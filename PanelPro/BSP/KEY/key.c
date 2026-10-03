#include "key.h"
#include "user_TaskInit.h"
#include "delay.h"

// uint8_t KeyScan(uint8_t mode)
// {
//     static uint8_t key_up = 1; // 按键松开标志
//     static uint8_t key_down = 0; // 当前按键值
//     uint8_t keyvalue = 0; // 当前扫描到的按键值

//     if(mode) 
//     {
//         key_up = 1;  // 进入自动扫描模式，重置按键状态
//     }

//     if(key_up && (!KEY1))
//     {
//         osDelay(3); // 消抖延时
//         if(!KEY1)
//         {
//             key_down = 1; // KEY1被按下
//         }
//         if(keyvalue)key_up = 0; // 如果有按键被按下，标记按键未松开
//     }
//     else
//     {
//         osDelay(3); // 消抖延时
//         if(KEY1)
//         {
//             key_up = 1; // KEY1被松开
//         }
//     }
//     return keyvalue; // 返回当前按键值
// }

uint8_t KeyScan(uint8_t mode)
{
    static uint8_t key_up = 1;  // 按键松开标志
    uint8_t keyvalue = 0;
    
    // 读取当前按键状态（注意：KEY1 宏定义读取的是 GPIO 电平）
    uint8_t key_state = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
    
    if(mode == 0)  // 单次触发模式
    {
        if(key_up && (key_state == GPIO_PIN_RESET))  // 按下
        {
            osDelay(3);  // 消抖
            if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
            {
                keyvalue = 1;
                key_up = 0;  // 标记已按下
            }
        }
        else if(key_state == GPIO_PIN_SET)  // 松开
        {
            key_up = 1;
        }
    }
    else  // 连按模式
    {
        if(key_state == GPIO_PIN_RESET)
        {
            osDelay(3);
            if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
            {
                keyvalue = 1;
            }
        }
    }
    
    return keyvalue;
}
