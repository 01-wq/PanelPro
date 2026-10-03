#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "semphr.h"
#include "tim.h"
#include "user_MotorTask.h"
#include "user_TaskInit.h"

#define PWM_MAX       99    // 最大占空比
#define PWM_MIN_START 10    // 最小启动占空比
#define ENCODER_SENSITIVITY_DIV 4  // 每变化 4 个计数才增减 1 个 duty

extern osMessageQueueId_t Motor_SpeedQueue;

SemaphoreHandle_t xMutex_Encoder_PWM;
void Encoder_PWM_Task(void *argument)
{
    xMutex_Encoder_PWM = xSemaphoreCreateMutex();       //创建互斥锁
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);       // 编码器定时器
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);             // PWM 定时器

    int16_t last_count = 0;//16位定时器
    last_count = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);	//把残留的数取出
    int32_t target_speed = 0;   	//目标速度
    static int32_t acc = 0;       //余数累加器
    int32_t scaled_delta = 0; 

    // 用于接收LVGL消息的变量
    int32_t ui_speed_percent = -1;   // -1表示未收到UI消息
    /* Infinite loop */
    for(;;)
    {
        // ========== 1. 接收来自LVGL UI的速度值 ==========
        int32_t received_speed;
        if(osMessageQueueGet(Motor_SpeedQueue, &received_speed, 0, 0) == osOK)
        {
            // 将0-100的百分比映射到 -PWM_MAX 到 PWM_MAX
            // 注意：这里假设UI只控制正转，如果你需要反转可以添加方向按钮
            if(received_speed >= 0 && received_speed <= 100)
            {
                // 线性映射：0% -> 0, 100% -> PWM_MAX
                int32_t new_target = (received_speed * PWM_MAX) / 100;
                
                // 加锁后更新目标速度
                if(xSemaphoreTake(xMutex_Encoder_PWM, portMAX_DELAY) == pdTRUE)
                {
                    target_speed = new_target;
                    xSemaphoreGive(xMutex_Encoder_PWM);
                }
            }
        }
        // ========== 2. 编码器调节==========
        int16_t count = __HAL_TIM_GET_COUNTER(&htim3);
        int32_t  delta = (int32_t)(count - last_count); //接完16位数据后转换成32位数据
        last_count = count;
        if(delta != 0)
        {
            if(xSemaphoreTake (xMutex_Encoder_PWM,portMAX_DELAY) == pdTRUE)//判断是否创建成功
            {//进入临界区
                acc += delta;
                scaled_delta = acc / ENCODER_SENSITIVITY_DIV;
                acc %= ENCODER_SENSITIVITY_DIV;
                xSemaphoreGive(xMutex_Encoder_PWM);
            }//退出临界区
            int32_t new_target = target_speed + scaled_delta;
            if (new_target > PWM_MAX) {
                target_speed = PWM_MAX;
            } else if (new_target < -PWM_MAX) {
                target_speed = -PWM_MAX;
            } else {
                target_speed = new_target;
            }
        }

        // ========== 3. 执行电机控制 ==========
        if(target_speed > 0)
        {
            HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);
            
            uint32_t duty = (uint32_t)target_speed;
            if(duty < PWM_MIN_START) duty = 0;   
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, duty);
        }
        else if(target_speed < 0)
        {
            HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_SET);
            
            uint32_t duty = (uint32_t)(-target_speed);
            if(duty < PWM_MIN_START) duty = 0;
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, duty);
        }
        else
        {
            HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 0);
        }
    osDelay(10);
    }
}
