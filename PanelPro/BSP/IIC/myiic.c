#include "myiic.h"

/**
  * @brief SDA线输入模式配置
  * @param None
  * @retval None
  */
void SDA_Input_Mode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.Pin = IIC_SDA_Pin;
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.Pull = GPIO_PULLUP;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(IIC_SDA_GPIO_Port, &GPIO_InitStructure);
}

/**
  * @brief SDA线输出模式配置
  * @param None
  * @retval None
  */
void SDA_Output_Mode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.Pin = IIC_SDA_Pin;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStructure.Pull = GPIO_PULLUP;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(IIC_SDA_GPIO_Port, &GPIO_InitStructure);
}

/**
 * @brief SDA输出一个位
 * @param val 输出的数据
 * @retval None
 */
void SDA_OUT(uint16_t val)
{
    if(val)
        IIC_SDA_GPIO_Port->BSRR |= IIC_SDA_Pin;
    else
        IIC_SDA_GPIO_Port->BSRR = IIC_SDA_Pin <<16U;
}

/**
  * @brief SCL线输出一个位
  * @param val 输出的数据
  * @retval None
  */
void SCL_OUT(uint16_t val)
{
    if(val)
        IIC_SCL_GPIO_Port->BSRR |= IIC_SCL_Pin;
    else
        IIC_SCL_GPIO_Port->BSRR = IIC_SCL_Pin <<16U;
}

/**
  * @brief SDA输入一位
  * @param None
  * @retval GPIO读入一位
  */
uint8_t SDA_IN(void)
{
    if(HAL_GPIO_ReadPin(IIC_SDA_GPIO_Port, IIC_SDA_Pin) == GPIO_PIN_SET)
        return 1;
    else
        return 0;
}
/*****************************************************************************
 * @name       :void IIC_Start(void)
 * @date       :2018-08-09 
 * @function   :Generating IIC starting signal
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void IIC_Start(void)
{
    SDA_Output_Mode(); //SDA线输出
	SDA_OUT(1);     
	SCL_OUT(1);
	delay_us(4);
 	SDA_OUT(0);//START:when CLK is high,DATA change form high to low 
	delay_us(4);
	SCL_OUT(0);//钳住I2C总线，准备发送或接收数据 
}

/*****************************************************************************
 * @name       :void IIC_Stop(void)
 * @date       :2018-08-09 
 * @function   :Generating IIC stop signal
 * @parameters :None
 * @retvalue   :None
******************************************************************************/  
void IIC_Stop(void)
{
    SDA_Output_Mode(); //SDA线输出
	SCL_OUT(0);
	SDA_OUT(0);//STOP:when CLK is high DATA change form low to high
 	delay_us(4);
	SCL_OUT(1); 
	SDA_OUT(1);//发送I2C总线结束信号
	delay_us(4);							   	
}

/*****************************************************************************
 * @name       :u8 IIC_Wait_Ack(void)
 * @date       :2018-08-09 
 * @function   :Wait for the response signal
 * @parameters :None
 * @retvalue   :0-receive response signal successfully
								1-receive response signal unsuccessfully
******************************************************************************/ 
u8 IIC_Wait_Ack(void)
{
	u8 ucErrTime=0;  
    SDA_Input_Mode();      //SDA设置为输入
	SDA_OUT(1);delay_us(1);	   
	SCL_OUT(1);delay_us(1);	 
	while(SDA_IN())//等待ACK信号到来
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
			IIC_Stop();
			return 1;
		}
	}
	SCL_OUT(0);//时钟输出0 	   
	return 0;  
}


/*****************************************************************************
 * @name       :void IIC_Ack(void)
 * @date       :2018-08-09 
 * @function   :Generate ACK response signal
 * @parameters :None
 * @retvalue   :None
******************************************************************************/ 
void IIC_Ack(void)
{
	SCL_OUT(0);
    SDA_Output_Mode(); //SDA线输出
	SDA_OUT(0);
	delay_us(2);
	SCL_OUT(1);
	delay_us(2);
	SCL_OUT(0);
}

/*****************************************************************************
 * @name       :void IIC_NAck(void)
 * @date       :2018-08-09 
 * @function   :Don't generate ACK response signal
 * @parameters :None
 * @retvalue   :None
******************************************************************************/ 	    
void IIC_NAck(void)
{
	SCL_OUT(0);
	SDA_Output_Mode(); //SDA线输出
	SDA_OUT(1);
	delay_us(2);
	SCL_OUT(1);
	delay_us(2);
	SCL_OUT(0);
}

/*****************************************************************************
 * @name       :void IIC_Send_Byte(u8 txd)
 * @date       :2018-08-09 
 * @function   :send a byte data by IIC bus
 * @parameters :txd:Data to be sent
 * @retvalue   :None
******************************************************************************/				 				     	  
void IIC_Send_Byte(u8 txd)
{                        
    u8 t;  
    SDA_Output_Mode(); //SDA线输出    
    SCL_OUT(0);//拉低时钟开始数据传输
    for(t=0;t<8;t++)
    {              
        SDA_OUT((txd&0x80)>>7);//送数据到SDA口
        txd<<=1; 	  
		delay_us(2);   //对TEA5767这三个延时都是必须的
		SCL_OUT(1);
		delay_us(2); 
		SCL_OUT(0);	
		delay_us(2);
    }	 
} 	

/*****************************************************************************
 * @name       :u8 IIC_Read_Byte(unsigned char ack)
 * @date       :2018-08-09 
 * @function   :read a byte data by IIC bus
 * @parameters :ack:0-send nACK
									  1-send ACK
 * @retvalue   :Data to be read
******************************************************************************/		    
u8 IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
    SDA_Input_Mode();      //SDA设置为输入	 
    for(i=0;i<8;i++ )
	{
        SCL_OUT(0); 
        delay_us(2);
		SCL_OUT(1);
        receive<<=1;
        if(SDA_IN())receive++;   
		delay_us(1); 
    }					 
    if (!ack)
        IIC_NAck();//发送nACK
    else
        IIC_Ack(); //发送ACK   
    return receive;
}

void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    IIC_CLK_ENABLE();
    
    /* 配置 SCL 为开漏输出 */
    GPIO_InitStructure.Pin = IIC_SCL_Pin;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;   // ✅ 开漏模式
    GPIO_InitStructure.Pull = GPIO_PULLUP;           // ✅ 启用上拉
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(IIC_SCL_GPIO_Port, &GPIO_InitStructure);
    
    /* 配置 SDA 为开漏输出 */
    GPIO_InitStructure.Pin = IIC_SDA_Pin;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;   // ✅ 开漏模式
    GPIO_InitStructure.Pull = GPIO_PULLUP;           // ✅ 启用上拉
    HAL_GPIO_Init(IIC_SDA_GPIO_Port, &GPIO_InitStructure);
}
