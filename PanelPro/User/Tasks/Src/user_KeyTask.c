#include "user_KeyTask.h"
#include "user_TaskInit.h"
#include "ui_HomePage.h"
#include "main.h"
#include "key.h"

/**
  * @brief  Key press check task
  * @param  argument: Not used
  * @retval None
  */
void KeyTask(void *argument)
{
  uint8_t keystr = 0;
  while (1)
  {
    switch (KeyScan(0))
    {
      case 1:
        keystr = 1;
        osMessageQueuePut(Key_MessageQueue,&keystr,0,1);
        break;
    }
    osDelay(1);
  }
}

