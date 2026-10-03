#include "main.h"
#include "user_TaskInit.h"
#include "rtc.h"
#include "user_RTCShowTask.h"

extern osMessageQueueId_t RTC_TimeQueue;

void RTC_Show_Task(void *argument)
{
	__HAL_RCC_PWR_CLK_ENABLE();
	HAL_PWR_EnableBkUpAccess();
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    RTC_Time_t rtc_data;
    
    for (;;) 
    {
        HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        
        rtc_data.hours = sTime.Hours;
        rtc_data.minutes = sTime.Minutes;
        rtc_data.seconds = sTime.Seconds;
        rtc_data.year = sDate.Year;
        rtc_data.month = sDate.Month;
        rtc_data.day = sDate.Date;
        
        // 发送到队列
        osMessageQueuePut(RTC_TimeQueue, &rtc_data, 0, 0);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
