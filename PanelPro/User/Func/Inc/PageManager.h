#ifndef PAGE_STACK_H
#define PAGE_STACK_H

#include "lvgl.h"

// 页面栈深度
#define MAX_DEPTH 6

// 页面结构体
typedef struct {
    void (*init)(void);   // 页面初始化函数
    void (*deinit)(void); // 页面销毁函数
    lv_obj_t * page;      // 页面对象
} Page_t;

// 页面堆栈结构体
typedef struct {
    Page_t * pages[MAX_DEPTH];
    uint8_t top;
} PageStack_t;

// 声明外部变量
extern PageStack_t PageStack;

// 函数声明
Page_t* Page_Get_NowPage(void);
void Page_Back(void);
void Page_Back_Bottom(void);
void Page_Load(Page_t *newPage);
void Pages_init(void);

#endif /* PAGE_STACK_H */
