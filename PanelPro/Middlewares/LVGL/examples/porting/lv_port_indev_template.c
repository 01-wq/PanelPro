/**
 * @file lv_port_indev_templ.c
 */

#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev_template.h"
#include "../../lvgl.h"
#include "touch.h"
#include <stdio.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad;
static uint32_t read_call_count = 0;
static uint32_t touch_count = 0;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;
    
    touchpad_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void touchpad_init(void)
{
    printf("[INIT] touchpad_init called\r\n");
    printf("[INIT] Calling tp_dev.init()...\r\n");
    tp_dev.init();
    printf("[INIT] tp_dev.init() completed\r\n");
}

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    static bool last_pressed = false;
    
    read_call_count++;
    
    /* 每 200 次调用打印一次 */
    if(read_call_count % 200 == 1) {
        printf("[READ] Called %lu times, lv_tick=%lu\r\n", 
               read_call_count, lv_tick_get());
    }
    
    /* 扫描触摸屏 */
    tp_dev.scan(0);
    
    /* 检查触摸（排除无效坐标）*/
    if((tp_dev.sta & TP_PRES_DOWN) && tp_dev.x[0] < 400) {
        last_x = tp_dev.x[0];
        last_y = tp_dev.y[0];
        
        /* 限制坐标范围 */
        if(last_x > 319) last_x = 319;
        if(last_y > 239) last_y = 239;
        
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = last_x;
        data->point.y = last_y;
        
        if(!last_pressed) {
            touch_count++;
            printf("[TOUCH] #%lu: PRESSED at (%d, %d), sta=0x%02X\r\n", 
                   touch_count, last_x, last_y, tp_dev.sta);
            last_pressed = true;
        }
    } 
    else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = last_x;
        data->point.y = last_y;
        
        if(last_pressed) {
            printf("[TOUCH] RELEASED\r\n");
            last_pressed = false;
        }
    }
}

#else
typedef int keep_pedantic_happy;
#endif
