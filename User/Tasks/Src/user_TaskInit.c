/* Private includes -----------------------------------------------------------*/
//includes
#include "user_TaskInit.h"
//sys
#include "sys.h"
#include "stdio.h"
#include "main.h"
#include "tim.h"

//bsp
//#include "key.h"
#include "Touch_Sensor.h"
//gui
#include "lvgl.h"

//tasks
#include "user_HardwareInitTask.h"
#include "user_LVGLTask.h"
#include "user_testTask.h"
#include "user_KeyTask.h"
#include "user_ScrRenewTask.h"
#include "user_MotorTask.h"
#include "user_LightTask.h"
#include "user_RTCShowTask.h"
/* Private typedef -----------------------------------------------------------*/


/* Private define ------------------------------------------------------------*/


/* Private variables ---------------------------------------------------------*/


/* Timers --------------------------------------------------------------------*/


/* Tasks ---------------------------------------------------------------------*/
// Hardwares initialization
osThreadId_t HardwareInitTaskHandle;
const osThreadAttr_t HardwareInitTask_attributes = {
  .name = "HardwareInitTask",
  .stack_size = 128 * 10,
  .priority = (osPriority_t) osPriorityHigh,
};

osThreadId_t TTP223_TaskHandle;
const osThreadAttr_t TTP223_Task_attributes= {
  .name = "TTP223_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

// LVGL Handler task
osThreadId_t LvHandlerTaskHandle;
const osThreadAttr_t LvHandlerTask_attributes = {
  .name = "LvHandlerTask",
  .stack_size = 4096,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t user_testTaskHandle;
const osThreadAttr_t testTask_attributes = {
  .name = "user_testTask",
  .stack_size = 8192,
  .priority = (osPriority_t) osPriorityNormal,
};

// Key task
osThreadId_t KeyTaskHandle;
const osThreadAttr_t KeyTask_attributes = {
  .name = "KeyTask",
  .stack_size = 128 * 1,
  .priority = (osPriority_t) osPriorityNormal,
};

//ScrRenew task
osThreadId_t ScrRenewTaskHandle;
const osThreadAttr_t ScrRenewTask_attributes = {
  .name = "ScrRenewTask",
  .stack_size = 128 * 10,
  .priority = (osPriority_t) osPriorityLow1,
};

osThreadId_t Encoder_PWM_TaskHandle;
const osThreadAttr_t Encoder_PWM_Task_attributes= {
  .name = "Encoder_PWM_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t RGB_led_TaskHandle;
const osThreadAttr_t RGB_led_Task_attributes= {
  .name = "RGB_led_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t RTC_Show_TaskHandle;
const osThreadAttr_t RTC_Show_Task_attributes= {
  .name = "RTC_Show_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Message queues ------------------------------------------------------------*/
osMessageQueueId_t Key_MessageQueue;
osMessageQueueId_t Motor_SpeedQueue;
osMessageQueueId_t RGB_ColorQueue;
osMessageQueueId_t RTC_TimeQueue;
osSemaphoreId_t DMA_SemaphoreHandle;
/* Private function prototypes -----------------------------------------------*/
void LvHandlerTask(void *argument);
void TTP223_Task(void *argument);
//void task(void *argument);
/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void User_Tasks_Init(void)
{
  /* add mutexes, ... */

  /* add semaphores, ... */

  /* start timers, add new ones, ... */

  /* add queues, ... */
  Key_MessageQueue = osMessageQueueNew(1,1,NULL);
  Motor_SpeedQueue = osMessageQueueNew(3,sizeof(int32_t),NULL);
  RGB_ColorQueue = osMessageQueueNew(5, sizeof(RGB_Color_t), NULL); 
  RTC_TimeQueue = osMessageQueueNew(5, sizeof(RTC_Time_t), NULL);
  DMA_SemaphoreHandle = osSemaphoreNew(1, 1, NULL);  // max=1, initial=1 (首次不阻塞)
	/* add threads, ... */
  HardwareInitTaskHandle  = osThreadNew(HardwareInitTask, NULL, &HardwareInitTask_attributes);
  LvHandlerTaskHandle     = osThreadNew(LvHandlerTask, NULL, &LvHandlerTask_attributes);
  //user_testTaskHandle     = osThreadNew(task, NULL, &testTask_attributes);
  KeyTaskHandle = osThreadNew(KeyTask,NULL,&KeyTask_attributes);
  ScrRenewTaskHandle = osThreadNew(ScrRenewTask,NULL,&ScrRenewTask_attributes);
  Encoder_PWM_TaskHandle = osThreadNew(Encoder_PWM_Task, NULL, &Encoder_PWM_Task_attributes);
  RGB_led_TaskHandle = osThreadNew(RGB_led_Task, NULL, &RGB_led_Task_attributes);
  RTC_Show_TaskHandle = osThreadNew(RTC_Show_Task, NULL, &RTC_Show_Task_attributes);
  TTP223_TaskHandle = osThreadNew(TTP223_Task, NULL, &TTP223_Task_attributes);
  /* add events, ... */

	/* add  others ... */


}

/**
  * @brief  FreeRTOS Tick Hook, to increase the LVGL tick
  * @param  None
  * @retval None
  */
void TaskTickHook(void)
{
  lv_tick_inc(1);
}

static void MyCallback(TTP223_Button *btn, TTP223_Event e);
static TTP223_Button key3,key2;
static float angle_target = 90;                //0-180
static float angle_current = 90;               
static const float speed_step = 10;  //转动角度的步进值
static int servo_step = 1; 			//平滑步进值
void TTP223_Task(void *argument)
{
  HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_3);
  TTP223_Init(&key3, KEY3_GPIO_Port, KEY3_Pin, 400, 400, MyCallback);
  TTP223_Init(&key2, KEY2_GPIO_Port, KEY2_Pin, 400, 400, MyCallback);
  /* Infinite loop */
  for(;;)
  {
    uint32_t now = HAL_GetTick();	

    TTP223_Scan(&key3, now);
    TTP223_Scan(&key2, now);
    if(angle_current < angle_target)
    {
      angle_current += servo_step;
      if(angle_current > angle_target) angle_current = angle_target;
    }
    else if(angle_current > angle_target)
    {
      angle_current -= servo_step;
      if(angle_current < angle_target) angle_current = angle_target;
    }
	 __HAL_TIM_SET_COMPARE(&htim5,TIM_CHANNEL_3,angle_current/180*2000 +500);
    osDelay(20);
  }

}
static void MyCallback(TTP223_Button *btn, TTP223_Event e)
{
  if (btn == &key3) 
  {
	  
       if (e == TTP223_EVENT_SHORT_PRESS)
		{
			angle_target +=speed_step;
		  if(angle_target>180){angle_target = 180;}
	}  
	else if (e == TTP223_EVENT_LONG_PRESS_START)
		{			
			angle_target+=speed_step;
				if(angle_target>180){angle_target = 180;}			
		}
        else if (e == TTP223_EVENT_LONG_PRESS_REPEAT)
		{
				angle_target+=speed_step*2;
				if(angle_target>180){angle_target = 180;}
		}
  }
  else if(btn == &key2)
  {
       if (e == TTP223_EVENT_SHORT_PRESS)
		{
      angle_target=angle_target-speed_step;
      if(angle_target<0){angle_target = 0;}
		}  
        else if(e == TTP223_EVENT_LONG_PRESS_START)
        {
			      angle_target-=speed_step;
            if(angle_target<0){angle_target = 0;}
        }
        else if(e == TTP223_EVENT_LONG_PRESS_REPEAT)
        {
          angle_target-=speed_step*2;
            if(angle_target<0){angle_target = 0;}
        }
  }
}
