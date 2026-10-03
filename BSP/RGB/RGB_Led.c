#include "RGB_Led.h"

void RGB_LED_Init(void)
{
    // 初始化 RGB LED 的 GPIO 引脚
    __HAL_RCC_GPIOA_CLK_ENABLE();                           // 使能 GPIO 时钟
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 配置 RGB LED 的引脚为输出模式
    GPIO_InitStruct.Pin = RGB_LED_PIN_R | RGB_LED_PIN_G | RGB_LED_PIN_B; 
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出
    GPIO_InitStruct.Pull = GPIO_NOPULL; // 无上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // 低速
    HAL_GPIO_Init(RGB_LED_PORT, &GPIO_InitStruct);

    // 初始状态关闭 RGB LED
    HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R | RGB_LED_PIN_G | RGB_LED_PIN_B, GPIO_PIN_RESET);
}

void change_color(uint8_t color)
{
    switch(color)
    {
        case color_red:
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R, GPIO_PIN_SET);
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_G | RGB_LED_PIN_B, GPIO_PIN_RESET);
            break;
        case color_green:
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_G, GPIO_PIN_SET);
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R | RGB_LED_PIN_B, GPIO_PIN_RESET);
            break;
        case color_blue:
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_B, GPIO_PIN_SET);
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R | RGB_LED_PIN_G, GPIO_PIN_RESET);
            break;
        case color_purple:
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R | RGB_LED_PIN_B, GPIO_PIN_SET);
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_G, GPIO_PIN_RESET);
            break;
        case color_yellow:
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R | RGB_LED_PIN_G, GPIO_PIN_SET);
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_B, GPIO_PIN_RESET);
            break;
        case color_cyan:
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_G | RGB_LED_PIN_B, GPIO_PIN_SET);
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R, GPIO_PIN_RESET);
            break;
        case color_white:
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R | RGB_LED_PIN_G | RGB_LED_PIN_B, GPIO_PIN_SET);
            break;
        default:
            // 关闭所有颜色
            HAL_GPIO_WritePin(RGB_LED_PORT, RGB_LED_PIN_R | RGB_LED_PIN_G | RGB_LED_PIN_B, GPIO_PIN_RESET);
    }
}
