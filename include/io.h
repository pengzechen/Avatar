
#ifndef __IO_H__
#define __IO_H__

#include "avatar_types.h"
#include "avatar_arg.h"

void
uart_early_init();
void
uart_early_putc(char c);
char
uart_early_getc();


void
io_early_init();
void
io_init();

char
getc();
void
putc(char c);


extern void
uart_putstr(const char *str);

/*  printf 函数库  */
extern int32_t
my_vprintf(const char *fmt, va_list va);
extern int32_t
my_snprintf(char *buf, int32_t size, const char *fmt, ...);
extern int32_t
my_vsnprintf(char *buf, int32_t size, const char *fmt, va_list va);

extern int32_t
logger(const char *fmt, ...);
extern int32_t
logger_info(const char *fmt, ...);
extern int32_t
logger_warn(const char *fmt, ...);
extern int32_t
logger_error(const char *fmt, ...);
extern int32_t
logger_debug(const char *fmt, ...);

extern int32_t
try_logger(const char *fmt, ...);
extern int32_t
try_logger_info(const char *fmt, ...);
extern int32_t
try_logger_warn(const char *fmt, ...);
extern int32_t
try_logger_error(const char *fmt, ...);
extern int32_t
try_logger_debug(const char *fmt, ...);

// 模块调试系统
#define DEBUG_MODULE_NONE   0
#define DEBUG_MODULE_GIC    1
#define DEBUG_MODULE_TASK   2
#define DEBUG_MODULE_VGIC   3
#define DEBUG_MODULE_VTIMER 4
#define DEBUG_MODULE_VPL011 5
#define DEBUG_MODULE_ALL    0xFF

extern void
set_debug_module(uint32_t module_mask);
extern uint32_t
get_debug_module(void);
extern int32_t
logger_module_debug(uint32_t module_id, const char *fmt, ...);
extern int32_t
try_logger_module_debug(uint32_t module_id, const char *fmt, ...);

// 便捷宏定义
#define logger_gic_debug(fmt, ...)    logger_module_debug(DEBUG_MODULE_GIC, fmt, ##__VA_ARGS__)
#define logger_task_debug(fmt, ...)   logger_module_debug(DEBUG_MODULE_TASK, fmt, ##__VA_ARGS__)
#define logger_vgic_debug(fmt, ...)   logger_module_debug(DEBUG_MODULE_VGIC, fmt, ##__VA_ARGS__)
#define logger_vtimer_debug(fmt, ...) logger_module_debug(DEBUG_MODULE_VTIMER, fmt, ##__VA_ARGS__)
#define logger_vpl011_debug(fmt, ...) logger_module_debug(DEBUG_MODULE_VPL011, fmt, ##__VA_ARGS__)

extern void
run_printf_tests();
#endif