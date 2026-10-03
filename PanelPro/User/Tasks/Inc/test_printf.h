#ifndef __TEST_PRINTF_H__
#define __TEST_PRINTF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/**
 * @brief  Debug print levels
 */
typedef enum {
    DBG_OFF   = 0,  // Silent
    DBG_ERR   = 1,  // Errors only
    DBG_WARN  = 2,  // Errors + Warnings
    DBG_INFO  = 3,  // Normal info
    DBG_DEBUG = 4,  // Verbose
} dbg_level_t;

/**
 * @brief  Set global debug level
 */
void dbg_set_level(dbg_level_t level);

/**
 * @brief  Get current debug level
 */
dbg_level_t dbg_get_level(void);

/**
 * @brief  Printf through RTT with level filtering
 * @note   Only prints if msg_level <= current global level
 */
void dbg_printf(dbg_level_t msg_level, const char *fmt, ...);

/* Convenience macros */
#define DBG_ERROR(fmt, ...)   dbg_printf(DBG_ERR,   "[ERR] " fmt, ##__VA_ARGS__)
#define DBG_WARN(fmt, ...)    dbg_printf(DBG_WARN,  "[WARN] " fmt, ##__VA_ARGS__)
#define DBG_INFO(fmt, ...)    dbg_printf(DBG_INFO,  "[INFO] " fmt, ##__VA_ARGS__)
#define DBG_DEBUG(fmt, ...)   dbg_printf(DBG_DEBUG, "[DBG] " fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __TEST_PRINTF_H__ */
