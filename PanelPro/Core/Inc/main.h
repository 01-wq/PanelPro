/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "spi.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY3_Pin GPIO_PIN_2
#define KEY3_GPIO_Port GPIOE
#define KEY1_Pin GPIO_PIN_3
#define KEY1_GPIO_Port GPIOE
#define T_CS_Pin GPIO_PIN_1
#define T_CS_GPIO_Port GPIOC
#define T_MISO_Pin GPIO_PIN_2
#define T_MISO_GPIO_Port GPIOC
#define T_MOSI_Pin GPIO_PIN_3
#define T_MOSI_GPIO_Port GPIOC
#define BL_Pin GPIO_PIN_0
#define BL_GPIO_Port GPIOA
#define CS_Pin GPIO_PIN_4
#define CS_GPIO_Port GPIOC
#define RESET_Pin GPIO_PIN_5
#define RESET_GPIO_Port GPIOC
#define DC_Pin GPIO_PIN_0
#define DC_GPIO_Port GPIOB
#define T_PEN_Pin GPIO_PIN_1
#define T_PEN_GPIO_Port GPIOB
#define T_PEN_EXTI_IRQn EXTI1_IRQn
#define KEY2_Pin GPIO_PIN_8
#define KEY2_GPIO_Port GPIOE
#define KEY2_EXTI_IRQn EXTI9_5_IRQn
#define T_SCK_Pin GPIO_PIN_10
#define T_SCK_GPIO_Port GPIOB
#define Red_Pin GPIO_PIN_8
#define Red_GPIO_Port GPIOD
#define Green_Pin GPIO_PIN_9
#define Green_GPIO_Port GPIOD
#define Blue_Pin GPIO_PIN_10
#define Blue_GPIO_Port GPIOD
#define MOTOR_AIN2_Pin GPIO_PIN_11
#define MOTOR_AIN2_GPIO_Port GPIOD
#define MOTOR_AIN1_Pin GPIO_PIN_12
#define MOTOR_AIN1_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
