#ifndef __MYIIC_H
#define __MYIIC_H

#include "sys.h"
#include "stm32f4xx.h"
#include "main.h"

#define IIC_CLK_ENABLE()   __HAL_RCC_GPIOB_CLK_ENABLE()

#define IIC_SCL_Pin        GPIO_PIN_11
#define IIC_SCL_GPIO_Port  GPIOB
#define IIC_SDA_Pin        GPIO_PIN_12
#define IIC_SDA_GPIO_Port  GPIOB

void IIC_Init(void);                //初始化IIC的IO口				 
void IIC_Start(void);				//发送IIC开始信号
void IIC_Stop(void);	  			//发送IIC停止信号
void IIC_Send_Byte(u8 txd);			//IIC发送一个字节
u8 IIC_Read_Byte(unsigned char ack);//IIC读取一个字节
u8 IIC_Wait_Ack(void); 				//IIC等待ACK信号
void IIC_Ack(void);					//IIC发送ACK信号
void IIC_NAck(void);				//IIC不发送ACK信号

#endif
