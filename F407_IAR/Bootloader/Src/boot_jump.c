#include "stm32f4xx.h"

/** @brief  跳转到APP
    @param  app_addr APP起始地址 
    @retval 无
    @note   跳转前需要关闭所有中断，并重新设置向量表
    */
void boot_jump_to_app(uint32_t app_addr)
{
    //关闭全部中断
    __disable_irq();

    //把向量表重新定位到APP的起始地址
    SCB->VTOR = app_addr;

    //从 app_addr 中读取初始栈指针
    uint32_t app_stack = *(volatile uint32_t *)app_addr;

    //从 app_addr + 4 中读取复位处理函数的地址
    uint32_t app_reset_handler = *(volatile uint32_t *)(app_addr + 4);

    //设MSP,跳转
    __set_MSP(app_stack);
    ((void (*)(void)) app_reset_handler)(); 
    while(1);  //如果跳转失败，进入死循环
}
