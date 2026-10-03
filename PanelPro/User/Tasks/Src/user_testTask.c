#include "user_testTask.h"
#include "cmsis_os2.h"
#include "ui.h"
#include "lcd.h"
#include "myspi.h"
#include "gui.h"
#include <stdio.h>

// 互斥锁句柄（与你的 LVGL 任务共用）
static osMutexId_t lvglMutexHandle = NULL;

static void lock(void)
{
    if (lvglMutexHandle != NULL) {
        osMutexAcquire(lvglMutexHandle, osWaitForever);
    }
}

static void unlock(void)
{
    if (lvglMutexHandle != NULL) {
        osMutexRelease(lvglMutexHandle);
    }
}

// 强制刷新显示
static void refresh_display(void)
{
    lv_timer_handler();
}

//void task(void *argument)
//{
//    
//    // 等待系统稳定
//    osDelay(500);
//    
//    lock();
//    
//    // ========== 直接加载 Menu Page 进行测试 ==========
//    
//    // 初始化 Menu Page
//    ui_MeunPage_screen_init();
//    
//    // 检查是否创建成功
//    if (ui_MeunPage != NULL) {
//        
//        // 启用滚动（如果还没有启用）
//        lv_obj_set_scroll_dir(ui_MeunPage, LV_DIR_VER);
//        lv_obj_set_scrollbar_mode(ui_MeunPage, LV_SCROLLBAR_MODE_AUTO);
//        
//        // 设置内容高度，确保可以滚动
//        // 内容总高度 = 最后一个面板位置 + 面板高度 + 底部留白
//        // 你的面板 Y 坐标：-380, -310, -240, -170
//        // 转换为相对顶部的距离需要加 120（屏幕中心）
//        // 大约需要 480 像素高度
//        lv_obj_set_height(ui_MeunPage, 480);
//        
//        
//        // 加载屏幕
//        lv_scr_load(ui_MeunPage);
//        refresh_display();
//        
//    } else {
//    }
//    
//    unlock();
//    
//    // 保持 LVGL 刷新
//    for (;;) {
//        lock();
//        refresh_display();
//        unlock();
//        osDelay(5);
//    }
//    vTaskDelete(NULL);
//}

