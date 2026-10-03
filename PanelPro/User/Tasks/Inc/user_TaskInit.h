#ifndef __USER_TASKSINIT_H__
#define __USER_TASKSINIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "cmsis_os.h"

// 使用条件编译避免重复定义
#ifndef RGB_COLOR_T_DEFINED
#define RGB_COLOR_T_DEFINED
// RGB颜色结构体
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB_Color_t;
#endif

extern osMessageQueueId_t Key_MessageQueue;
extern osMessageQueueId_t Motor_SpeedQueue;
extern osMessageQueueId_t RGB_ColorQueue;
extern osMessageQueueId_t RTC_TimeQueue;
extern osSemaphoreId_t DMA_SemaphoreHandle;
// 时间结构体
typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t year;
    uint8_t month;
    uint8_t day;
} RTC_Time_t;

void User_Tasks_Init(void);
void TaskTickHook(void);

#ifdef __cplusplus
}
#endif

#endif

