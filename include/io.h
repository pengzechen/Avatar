
#ifndef __IO_H__
#define __IO_H__

#include "aj_types.h"
#include "aj_arg.h"

void uart_early_init();
void uart_early_putc(char c);
char uart_early_getc();

void uart_advance_init();
char uart_advance_getc(void);
void uart_advance_putc(char c);

void io_early_init();
void io_init();
char getc();
void putc(char c);


extern void uart_putstr(const char *str);

/*  printf 函数库  */
extern int32_t my_vprintf(const char *fmt, va_list va);
extern int32_t my_snprintf(char *buf, int32_t size, const char *fmt, ...);
extern int32_t my_vsnprintf(char *buf, int32_t size, const char *fmt, va_list va);

extern int32_t logger(const char *fmt, ...);
extern int32_t logger_info(const char *fmt, ...);
extern int32_t logger_warn(const char *fmt, ...);
extern int32_t logger_error(const char *fmt, ...);
extern int32_t logger_debug(const char *fmt, ...);

extern int32_t try_logger(const char *fmt, ...);
extern int32_t try_logger_info(const char *fmt, ...);
extern int32_t try_logger_warn(const char *fmt, ...);
extern int32_t try_logger_error(const char *fmt, ...);
extern int32_t try_logger_debug(const char *fmt, ...);


extern void run_printf_tests();
#endif