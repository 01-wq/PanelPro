/* Private includes -----------------------------------------------------------*/

// includes
// sys
#include "delay.h"
#include "sys.h"

// user
#include "user_TaskInit.h"

// bsp
#include "lcd.h"
#include "touch.h"
#include "gui.h"
// ui
#include "lvgl.h"
#include "ui.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
//#include "ui.h"
#include "PageManager.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/


/**
  * @brief  hardwares init task
  * @param  argument: Not used
  * @retval None
  */
void HardwareInitTask(void *argument)
{
	while(1)
	{
		vTaskSuspendAll();

		delay_init();

		LCD_Init();
		TP_Init();

		// ui
		// LVGL and disp init
		lv_init();
		lv_port_disp_init();

		lv_port_indev_init();
		ui_init();

		Pages_init();
		xTaskResumeAll();
		vTaskDelete(NULL);
	}
}


