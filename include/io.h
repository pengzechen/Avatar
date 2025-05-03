
#ifndef __IO_H__
#define __IO_H__

#include "aj_types.h"
#include "stdarg.h"
#include "mem/barrier.h"

static inline uint8_t read8(const volatile void *addr) {
    uint8_t val;
    __asm__ volatile ("ldrb %w0, [%1]" : "=r" (val) : "r" (addr));
    return val;
}

static inline void write8(uint8_t value, volatile void *addr) {
    __asm__ volatile ("strb %w0, [%1]" :: "r" (value), "r" (addr));
}

static inline uint16_t read16(const volatile void *addr) {
    uint16_t val;
    __asm__ volatile ("ldrh %w0, [%1]" : "=r" (val) : "r" (addr));
    return val;
}

static inline void write16(uint16_t value, volatile void *addr) {
    __asm__ volatile ("strh %w0, [%1]" :: "r" (value), "r" (addr));
}

static inline uint32_t read32(const volatile void *addr) {
    uint32_t val;
    __asm__ volatile ("ldr %w0, [%1]" : "=r" (val) : "r" (addr));
    return val;
}

static inline void write32(uint32_t value, volatile void *addr) {
    __asm__ volatile ("str %w0, [%1]" :: "r" (value), "r" (addr));
}

static inline uint64_t read64(const volatile void *addr) {
    uint64_t val;
    __asm__ volatile ("ldr %0, [%1]" : "=r" (val) : "r" (addr));
    return val;
}

static inline void write64(uint64_t value, volatile void *addr) {
    __asm__ volatile ("str %0, [%1]" :: "r" (value), "r" (addr));
}


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


/*  printf 函数库  */
extern void uart_putstr(const char * str);
#define puts uart_putstr

extern int printf(const char *fmt, ...);
extern void print_info(const char *info);
extern void print_warn(const char *info);
extern int warning(const char *fmt, ...);
extern int error(const char *fmt, ...);
extern int snprintf(char *buf, int size, const char *fmt, ...);
extern int vsnprintf(char *buf, int size, const char *fmt, va_list va);



#endif