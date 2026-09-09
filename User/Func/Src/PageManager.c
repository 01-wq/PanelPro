#include "PageManager.h"
#include "ui_HomePage.h"
#include "ui_MeunPage.h"

PageStack_t PageStack;

// 初始化堆栈
static void page_stack_init(PageStack_t * stack) 
{
    stack->top = 0;  // 栈顶指针指向0，表示空栈
}

static uint8_t page_stack_push(PageStack_t * stack, Page_t * page) 
{
    if(stack->top >= MAX_DEPTH)  // 检查栈是否已满
    {
        return -1; 
    }
    stack->pages[stack->top++] = page;  // 存入页面,top自增
    return 0;
}

// 弹出页面
static uint8_t page_stack_pop(PageStack_t * stack) 
{
    if(stack->top <= 0) // 检查栈是否是空的
    {
        return -1; 
    }
    stack->top--;
    if(stack->pages[stack->top] != NULL && stack->pages[stack->top]->deinit != NULL) {
        stack->pages[stack->top]->deinit();  // 删除页面
    }
    return 0;
}

static uint8_t page_stack_is_empty(const PageStack_t * stack) 
{
    return stack->top == 0; // 如果栈顶指针为0，表示栈空
}

// 获取栈顶页面
static Page_t* get_top_page(const PageStack_t * stack) 
{
    if(page_stack_is_empty(stack)) 
    {
        return NULL; // 栈空时返回NULL
    }
    return stack->pages[stack->top - 1]; // 返回栈顶页面
}

/**
 * 获取当前页面（栈顶页面）的指针。
 * @param NULL
 * @return 返回当前页面的指针，如果栈为空则返回NULL。
 */
Page_t* Page_Get_NowPage(void)
{
    return get_top_page(&PageStack);
}

/**
 * back to previous page
 *
 * @param NULL
 * @return NULL
 */
void Page_Back(void)
{
    if(page_stack_is_empty(&PageStack))
    {
        return;
    }

    page_stack_pop(&PageStack); // 弹出当前页面

    if(page_stack_is_empty(&PageStack)) 
    {
        // 重新压入 Page_Home 和 Page_Menu（重建基础栈）
        page_stack_push(&PageStack, &Page_Home); 
        page_stack_push(&PageStack, &Page_Menu); 
        if(Page_Menu.init != NULL) {
            Page_Menu.init(); // 初始化菜单页面
        }
        if(Page_Menu.page != NULL) {
            lv_scr_load(Page_Menu.page); // 切换到菜单页面
        }
    }
    else
    {
        // 切换到上一个页面
        Page_t *previous_page = PageStack.pages[PageStack.top - 1];
        if(previous_page != NULL) {
            if(previous_page->init != NULL) {
                previous_page->init(); // 初始化上一个页面
            }
            if(previous_page->page != NULL) {
                lv_scr_load(previous_page->page); // 切换到上一个页面
            }
        }
    }
}

/**
 * back to bottom page home page
 *
 * @param NULL
 * @return NULL
 */
void Page_Back_Bottom(void)
{
    if(page_stack_is_empty(&PageStack))
    {
        return;
    }

    while (PageStack.top > 1)
    {
        page_stack_pop(&PageStack);
    }

    Page_t *bottom_page = PageStack.pages[PageStack.top - 1];

    if(bottom_page != NULL) {
        if(bottom_page->init != NULL) {
            bottom_page->init();
        }
        if(bottom_page->page != NULL) {
            lv_scr_load(bottom_page->page);
        }
    }
}

/**
 * Load a new page to stack top
 *
 * @param newPage Page_t a new page
 * @return NULL
 */
//void Page_Load(Page_t *newPage) 
//{
//    // 检查参数有效性
//    if(newPage == NULL) return;
//    
//    // 检查堆栈是否已满
//    if (PageStack.top >= MAX_DEPTH - 1) 
//    {
//        // 错误处理：堆栈满
//        return;
//    }

//    // 如果堆栈非空，反初始化当前页面
//    if (PageStack.top > 0 && PageStack.pages[PageStack.top - 1] != NULL) 
//    {
//        if(PageStack.pages[PageStack.top - 1]->deinit != NULL) {
//            PageStack.pages[PageStack.top - 1]->deinit();
//        }
//    }

//    // 将新页面推入堆栈
//    page_stack_push(&PageStack, newPage);
//    if(newPage->init != NULL) {
//        newPage->init(); // 初始化新页面
//    }
//    if(newPage->page != NULL) {
//        lv_scr_load_anim(newPage->page, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true); // 加载并应用动画
//    }
//}

void Page_Load(Page_t *newPage) 
{
    // 检查参数有效性
    if(newPage == NULL) 
	{
        return;
    }
    
    // 检查堆栈是否已满
    if (PageStack.top >= MAX_DEPTH - 1) 
    {
        return;
    }

    // 将新页面推入堆栈
    page_stack_push(&PageStack, newPage);
    
    // 初始化新页面
    if(newPage->init != NULL) 
	{
        newPage->init();
    }
    
    // 加载屏幕（改用 lv_scr_load，不用动画）
    if(newPage->page != NULL) 
	{
        lv_scr_load(newPage->page);  // 改为这个
        // lv_scr_load_anim(newPage->page, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    }
}

/**
 * initialize the page manager & start home page
 *
 * @param NULL
 * @return NULL
 */
void Pages_init(void) 
{
    // 强制确保有一个活动屏幕
    if(lv_scr_act() == NULL) {
        lv_obj_t *default_scr = lv_obj_create(NULL);
        lv_disp_load_scr(default_scr);
    }
    
    page_stack_init(&PageStack);
    page_stack_push(&PageStack, &Page_Home);
    if(Page_Home.init != NULL) {
        Page_Home.init();
    }
    if(Page_Home.page != NULL) {
        lv_disp_load_scr(Page_Home.page);
    }
}
