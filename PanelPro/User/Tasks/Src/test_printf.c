/**
 * @file    test_printf.c
 * @brief   Centralized debug printf via SEGGER RTT
 *
 * Usage:
 *   1. Call dbg_set_level(DBG_INFO) once at startup
 *   2. Use DBG_ERROR/DBG_WARN/DBG_INFO/DBG_DEBUG macros
 *   3. Set level to DBG_OFF for production (no output at all)
 */

#include "test_printf.h"
#include "SEGGER_RTT.h"
#include <stdarg.h>

static dbg_level_t g_dbg_level = DBG_INFO;  // default: show INFO and above

void dbg_set_level(dbg_level_t level)
{
    g_dbg_level = level;
}

dbg_level_t dbg_get_level(void)
{
    return g_dbg_level;
}

void dbg_printf(dbg_level_t msg_level, const char *fmt, ...)
{
    if (msg_level > g_dbg_level)
        return;  // filtered out

    va_list args;
    va_start(args, fmt);
    SEGGER_RTT_vprintf(0, fmt, &args);
    va_end(args);
}
