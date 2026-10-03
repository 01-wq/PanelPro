#include "user_TaskInit.h"
#include "user_ScrRenewTask.h"
#include "main.h"
#include "lvgl.h"
#include "ui_HomePage.h"
#include "ui_MeunPage.h"

extern osMessageQueueId_t Key_MessageQueue;

/**
  * @brief  Screen renew task
  * @param  argument: Not used
  * @retval None
  */
void ScrRenewTask(void *argument)
{
    uint8_t keystr = 0;
    while (1)
    {
        if(osMessageQueueGet(Key_MessageQueue,&keystr,NULL,0) == osOK)
        {
            if(keystr == 1)
            {
                Page_t *current_page = Page_Get_NowPage();
                
                // 情况1：在 HomePage，跳转到 MenuPage
                if(current_page == &Page_Home)
                {
                    Page_Load(&Page_Menu);
                }
                // 情况2：在 MenuPage，返回 HomePage
                else if(current_page == &Page_Menu)
                {
                    Page_Back_Bottom();     
                }
                // 情况3：在其他子页面，返回上一页（MenuPage）
                else
                {
                    Page_Back();
                }
            }
        }
        osDelay(10);
    }   
}
