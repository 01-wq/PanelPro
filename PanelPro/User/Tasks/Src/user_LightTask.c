#include "user_LightTask.h"
#include "user_TaskInit.h"
#include "RGB_Led.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <stdio.h>

extern osMessageQueueId_t RGB_ColorQueue;

// RGB值到7种颜色的映射函数（与UI层保持一致）
static uint8_t map_rgb_to_7color(uint8_t r, uint8_t g, uint8_t b)
{
    // 判断白色（RGB都大于180）
    if(r > 180 && g > 180 && b > 180)
    {
        return color_white;
    }
    // 判断红色（R最强且R>G+B，或R>150且G<100且B<100）
    else if(r > 150 && g < 100 && b < 100)
    {
        return color_red;
    }
    // 判断绿色（G最强且G>R+B，或G>150且R<100且B<100）
    else if(g > 150 && r < 100 && b < 100)
    {
        return color_green;
    }
    // 判断蓝色（B最强且B>R+G，或B>150且R<100且G<100）
    else if(b > 150 && r < 100 && g < 100)
    {
        return color_blue;
    }
    // 判断黄色（R和G强，B弱）
    else if(r > 150 && g > 150 && b < 100)
    {
        return color_yellow;
    }
    // 判断紫色（R和B强，G弱）
    else if(r > 150 && b > 150 && g < 100)
    {
        return color_purple;
    }
    // 判断青色（G和B强，R弱）
    else if(g > 150 && b > 150 && r < 100)
    {
        return color_cyan;
    }
    
    // 默认返回红色
    return color_red;
}

/**
* @brief Function implementing the RGB_led_Task thread.
* @param argument: Not used
* @retval None
*/
void RGB_led_Task(void *argument)
{
  /* USER CODE BEGIN RGB_led_Task */
    RGB_LED_Init();
  // 默认颜色：红色
    // 默认颜色：红色
    uint8_t current_color_enum = color_red;
    RGB_Color_t received_color;
  /* Infinite loop */
  for(;;)
  {
    if(osMessageQueueGet(RGB_ColorQueue,&received_color,NULL,pdMS_TO_TICKS(100)) == osOK)
    {
        // 映射到7种颜色
        current_color_enum = map_rgb_to_7color(received_color.red, received_color.green,received_color.blue);
    }

    // 调用原有函数改变LED颜色
    change_color(current_color_enum);

    osDelay(50);
  }
  /* USER CODE END RGB_led_Task */
}
