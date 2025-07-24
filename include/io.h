
#ifndef __IO_H__
#define __IO_H__

#include "aj_types.h"
#include "stdarg.h"

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
extern int my_vprintf(const char *fmt, va_list va);
extern int my_snprintf(char *buf, int size, const char *fmt, ...);
extern int my_vsnprintf(char *buf, int size, const char *fmt, va_list va);

extern int logger(const char *fmt, ...);
extern int logger_info(const char *fmt, ...);
extern int logger_warn(const char *fmt, ...);
extern int logger_error(const char *fmt, ...);

extern void run_printf_tests();
#endif