/* Private includes -----------------------------------------------------------*/
//includes

//bsp

// tasks
#include "user_TaskInit.h"
#include "user_LVGLTask.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

//gui
#include "lvgl.h"
//#include "ui.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  FreeRTOS Tick Hook, to increase the LVGL tick
  * @param  None
  * @retval None
  */
//void TaskTickHook(void)
//{
//	//to increase the LVGL tick
//	lv_tick_inc(1);
//}


/**
  * @brief  LVGL Handler task, to run the lvgl
  * @param  argument: Not used
  * @retval None
  */
void LvHandlerTask(void *argument)
{
    uint32_t _time = 1;
    
    while(1)
    {
        _time = lv_timer_handler();
        osDelay(_time);
    }
}
