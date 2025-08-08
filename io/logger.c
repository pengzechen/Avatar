/*
 * libc printf and friends
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License version 2.
 */


#include <aj_arg.h>
#include <aj_types.h>
#include <lib/aj_string.h>
#include <io.h>
#include <spinlock.h>

#ifndef __LOG_LEVEL
#define __LOG_LEVEL 3
#endif

#define LOG_LEVEL_DEBUG   0
#define LOG_LEVEL_INFO    1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_ERROR   3
#define LOG_LEVEL_NONE    4

// 模块调试系统
#define DEBUG_MODULE_NONE    0
#define DEBUG_MODULE_GIC     1
#define DEBUG_MODULE_TASK    2
#define DEBUG_MODULE_VGIC    3
#define DEBUG_MODULE_TIMER   4
#define DEBUG_MODULE_MEM     5
#define DEBUG_MODULE_ALL     0xFF

#ifndef __DEBUG_MODULE
#define __DEBUG_MODULE DEBUG_MODULE_NONE
#endif

#define BINSTR_SZ (sizeof(uint32_t) * 8 + sizeof(uint32_t) * 2)

#define BUFSZ 512

#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_RESET   "\x1b[0m"


static spinlock_t print_lock;

static uint64_t missed_log_plain = 0;
static uint64_t missed_log_debug = 0;
static uint64_t missed_log_info = 0;
static uint64_t missed_log_warn = 0;
static uint64_t missed_log_error = 0;

// 模块调试控制
static uint32_t debug_module_mask = __DEBUG_MODULE;


static char digits[16] = "0123456789abcdef";

typedef struct pstream {
    char *buffer;
    int32_t remain;
    int32_t added;
} pstream_t;

typedef struct strprops {
    char pad;
    int32_t npad;
    bool alternate;
} strprops_t;

static void addchar(pstream_t *p, char c)
{
    if (p->remain)
    {
        *p->buffer++ = c;
        --p->remain;
    }
    ++p->added;
}

static void print_str(pstream_t *p, const char *s, strprops_t props)
{
    const char *s_orig = s;
    int32_t npad = props.npad;

    if (npad > 0)
    {
        npad -= strlen(s_orig);
        while (npad > 0)
        {
            addchar(p, props.pad);
            --npad;
        }
    }

    while (*s)
        addchar(p, *s++);

    if (npad < 0)
    {
        props.pad = ' '; /* ignore '0' flag with '-' flag */
        npad += strlen(s_orig);
        while (npad < 0)
        {
            addchar(p, props.pad);
            ++npad;
        }
    }
}

static void print_int(pstream_t *ps, int64_t n, int32_t base, strprops_t props)
{
    char buf[sizeof(long) * 3 + 2], *p = buf;
    int32_t s = 0, i;

    if (n < 0)
    {
        n = -n;
        s = 1;
    }

    while (n)
    {
        *p++ = digits[n % base];
        n /= base;
    }

    if (s)
        *p++ = '-';

    if (p == buf)
        *p++ = '0';

    for (i = 0; i < (p - buf) / 2; ++i)
    {
        char tmp;
        tmp = buf[i];
        buf[i] = p[-1 - i];
        p[-1 - i] = tmp;
    }

    *p = 0;

    print_str(ps, buf, props);
}

static void print_unsigned(pstream_t *ps, uint64_t n, int32_t base, strprops_t props)
{
    char buf[sizeof(long) * 3 + 3], *p = buf;
    int32_t i;

    while (n)
    {
        *p++ = digits[n % base];
        n /= base;
    }

    if (p == buf)
        *p++ = '0';
    else if (props.alternate && base == 16)
    {
        if (props.pad == '0')
        {
            addchar(ps, '0');
            addchar(ps, 'x');

            if (props.npad > 0)
                props.npad = MAX(props.npad - 2, 0);
        }
        else
        {
            *p++ = 'x';
            *p++ = '0';
        }
    }

    for (i = 0; i < (p - buf) / 2; ++i)
    {
        char tmp;
        tmp = buf[i];
        buf[i] = p[-1 - i];
        p[-1 - i] = tmp;
    }

    *p = 0;

    print_str(ps, buf, props);
}

static int32_t fmtnum(const char **fmt)
{
    const char *f = *fmt;
    int32_t len = 0, num;

    if (*f == '-')
        ++f, ++len;

    while (*f >= '0' && *f <= '9')
        ++f, ++len;

    num = atol(*fmt);
    *fmt += len;
    return num;
}

int32_t my_vsnprintf(char *buf, int32_t size, const char *fmt, va_list va)
{
    pstream_t s;
    s.buffer = buf;
    s.remain = size - 1;
    s.added = 0;
    while (*fmt)
    {
        char f = *fmt++;
        int32_t nlong = 0;
        strprops_t props;
        memset(&props, 0, sizeof(props));
        props.pad = ' ';

        if (f != '%')
        {
            addchar(&s, f);
            continue;
        }
    morefmt:
        f = *fmt++;
        switch (f)
        {
        case '%':
            addchar(&s, '%');
            break;
        case 'c':
            addchar(&s, va_arg(va, int32_t));
            break;
        case '\0':
            --fmt;
            break;
        case '#':
            props.alternate = true;
            goto morefmt;
        case '0':
            props.pad = '0';
            ++fmt;
            /* fall through */
        case '1' ... '9':
        case '-':
            --fmt;
            props.npad = fmtnum(&fmt);
            goto morefmt;
        case 'l':
            ++nlong;
            goto morefmt;
        case 't':
        case 'z':
            /* Here we only care that sizeof(size_t) == sizeof(long).
             * On a 32-bit platform it doesn't matter that size_t is
             * typedef'ed to int32_t or long; va_arg will work either way.
             * Same for ptrdiff_t (%td).
             */
            nlong = 1;
            goto morefmt;
        case 'd':
            switch (nlong)
            {
            case 0:
                print_int(&s, va_arg(va, int32_t), 10, props);
                break;
            case 1:
                print_int(&s, va_arg(va, long), 10, props);
                break;
            default:
                print_int(&s, va_arg(va, long long), 10, props);
                break;
            }
            break;
        case 'u':
            switch (nlong)
            {
            case 0:
                print_unsigned(&s, va_arg(va, unsigned), 10, props);
                break;
            case 1:
                print_unsigned(&s, va_arg(va, unsigned long), 10, props);
                break;
            default:
                print_unsigned(&s, va_arg(va, unsigned long long), 10, props);
                break;
            }
            break;
        case 'x':
            switch (nlong)
            {
            case 0:
                print_unsigned(&s, va_arg(va, unsigned), 16, props);
                break;
            case 1:
                print_unsigned(&s, va_arg(va, unsigned long), 16, props);
                break;
            default:
                print_unsigned(&s, va_arg(va, unsigned long long), 16, props);
                break;
            }
            break;
        case 'p':
            props.alternate = true;
            print_unsigned(&s, (unsigned long)va_arg(va, void *), 16, props);
            break;
        case 's':
            print_str(&s, va_arg(va, const char *), props);
            break;
        default:
            addchar(&s, f);
            break;
        }
    }
    *s.buffer = 0;
    return s.added;
}

int32_t my_snprintf(char *buf, int32_t size, const char *fmt, ...)
{
    va_list va;
    int32_t r;

    va_start(va, fmt);
    r = my_vsnprintf(buf, size, fmt, va);
    va_end(va);
    return r;
}

int32_t my_vprintf(const char *fmt, va_list va)
{
    char buf[BUFSZ];
    int32_t r;

    r = my_vsnprintf(buf, sizeof(buf), fmt, va);
    uart_putstr(buf);
    return r;
}

static int32_t logger_impl(const char *color, const char *fmt, va_list va)
{
    spin_lock(&print_lock);
    char buf[BUFSZ];
    int32_t r = my_vsnprintf(buf, sizeof buf, fmt, va);

    if (color) uart_putstr(color);
    uart_putstr(buf);
    if (color) uart_putstr(ANSI_RESET);
    spin_unlock(&print_lock);

    return r;
}

static int32_t try_logger_impl(const char *color, uint64_t *missed_counter, const char *fmt, va_list va)
{
    if (!spin_trylock(&print_lock)) {
        if (missed_counter) (*missed_counter)++;
        return -1;
    }

    char buf[BUFSZ];
    int32_t r = my_vsnprintf(buf, sizeof buf, fmt, va);

    if (color) uart_putstr(color);
    uart_putstr(buf);
    if (color) uart_putstr(ANSI_RESET);

    spin_unlock(&print_lock);
    return r;
}

// --------- 普通阻塞接口 ---------


int32_t logger(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = logger_impl(NULL, fmt, va);
    va_end(va);
    return r;
}

#if __LOG_LEVEL <= LOG_LEVEL_DEBUG
int32_t logger_debug(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = logger_impl(ANSI_BLUE, fmt, va);
    va_end(va);
    return r;
}
#else
int32_t logger_debug(const char *fmt, ...) { (void)fmt; return 0; }
#endif

#if __LOG_LEVEL <= LOG_LEVEL_INFO
int32_t logger_info(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = logger_impl(ANSI_GREEN, fmt, va);
    va_end(va);
    return r;
}
#else
int32_t logger_info(const char *fmt, ...) { (void)fmt; return 0; }
#endif

#if __LOG_LEVEL <= LOG_LEVEL_WARN
int32_t logger_warn(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = logger_impl(ANSI_YELLOW, fmt, va);
    va_end(va);
    return r;
}
#else
int32_t logger_warn(const char *fmt, ...) { (void)fmt; return 0; }
#endif

#if __LOG_LEVEL <= LOG_LEVEL_ERROR
int32_t logger_error(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = logger_impl(ANSI_RED, fmt, va);
    va_end(va);
    return r;
}
#else
int32_t logger_error(const char *fmt, ...) { (void)fmt; return 0; }
#endif



// --------- 非阻塞 try_logger 接口 ---------


int32_t try_logger(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = try_logger_impl(NULL, &missed_log_plain, fmt, va);
    va_end(va);
    return r;
}

#if __LOG_LEVEL <= LOG_LEVEL_DEBUG
int32_t try_logger_debug(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = try_logger_impl(ANSI_BLUE, &missed_log_debug, fmt, va);
    va_end(va);
    return r;
}
#else
int32_t try_logger_debug(const char *fmt, ...) { (void)fmt; return 0; }
#endif

#if __LOG_LEVEL <= LOG_LEVEL_INFO
int32_t try_logger_info(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = try_logger_impl(ANSI_GREEN, &missed_log_info, fmt, va);
    va_end(va);
    return r;
}
#else
int32_t try_logger_info(const char *fmt, ...) { (void)fmt; return 0; }
#endif

#if __LOG_LEVEL <= LOG_LEVEL_WARN
int32_t try_logger_warn(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = try_logger_impl(ANSI_YELLOW, &missed_log_warn, fmt, va);
    va_end(va);
    return r;
}
#else
int32_t try_logger_warn(const char *fmt, ...) { (void)fmt; return 0; }
#endif

#if __LOG_LEVEL <= LOG_LEVEL_ERROR
int32_t try_logger_error(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int32_t r = try_logger_impl(ANSI_RED, &missed_log_error, fmt, va);
    va_end(va);
    return r;
}
#else
int32_t try_logger_error(const char *fmt, ...) { (void)fmt; return 0; }
#endif




// Dump the missed log stats
void log_stats_dump(void)
{
    logger("Missed logs: plain=%lu, info=%lu, warn=%lu, error=%lu\n",
        missed_log_plain, missed_log_info, missed_log_warn, missed_log_error);
}

void binstr(uint32_t x, char out[BINSTR_SZ])
{
    int32_t i;
    char *c;
    int32_t n;

    n = sizeof(uint32_t) * 8;
    i = 0;
    c = &out[0];
    for (;;)
    {
        *c++ = (x & (1ul << (n - i - 1))) ? '1' : '0';
        i++;

        if (i == n)
        {
            *c = '\0';
            break;
        }
        if (i % 4 == 0)
            *c++ = '\'';
    }
    // assert(c + 1 - &out[0] == BINSTR_SZ);
}

void print_binstr(uint32_t x)
{
    char out[BINSTR_SZ];
    binstr(x, out);
    logger("%s", out);
}

void run_printf_tests(void)
{
    char buf[64];

    // T1: print_int(0, ...) => 应该输出 "0"
    my_snprintf(buf, sizeof(buf), "%d", 0);
    uart_putstr("T1: expect [0] got [");
    uart_putstr(buf);
    uart_putstr("]\n");

    // T2: %#x 应该输出 "0x1a2b" 形式
    my_snprintf(buf, sizeof(buf), "%#x", 0x1a2b);
    uart_putstr("T2: expect [0x1a2b] got [");
    uart_putstr(buf);
    uart_putstr("]\n");

    // T3: 负数打印
    my_snprintf(buf, sizeof(buf), "%d", -12345);
    uart_putstr("T3: expect [-12345] got [");
    uart_putstr(buf);
    uart_putstr("]\n");

    // T4: %-10s 左对齐字符串
    my_snprintf(buf, sizeof(buf), "|%-10s|", "abc");
    uart_putstr("T4: expect [|abc       |] got [");
    uart_putstr(buf);
    uart_putstr("]\n");

    // T5: %% 测试
    my_snprintf(buf, sizeof(buf), "rate: 100%%");
    uart_putstr("T5: expect [rate: 100%] got [");
    uart_putstr(buf);
    uart_putstr("]\n");

    // T6: %08x 测试，检查是否前导 0 填充
    my_snprintf(buf, sizeof(buf), "%08x", 0x123);
    uart_putstr("T6: expect [00000123] got [");
    uart_putstr(buf);
    uart_putstr("]\n");
}

// --------- 模块调试控制函数 ---------

// 设置调试模块掩码
void set_debug_module(uint32_t module_mask)
{
    debug_module_mask = module_mask;
}

// 获取当前调试模块掩码
uint32_t get_debug_module(void)
{
    return debug_module_mask;
}

// 检查模块是否启用调试
static inline int32_t is_module_debug_enabled(uint32_t module_id)
{
    if (debug_module_mask == DEBUG_MODULE_ALL) {
        return 1;
    }
    return (debug_module_mask & (1 << module_id)) != 0;
}

// 模块化调试日志函数
int32_t logger_module_debug(uint32_t module_id, const char *fmt, ...)
{
#if __LOG_LEVEL <= LOG_LEVEL_DEBUG
    if (!is_module_debug_enabled(module_id)) {
        return 0;
    }

    va_list va;
    va_start(va, fmt);
    int32_t r = logger_impl(ANSI_BLUE, fmt, va);
    va_end(va);
    return r;
#else
    (void)module_id;
    (void)fmt;
    return 0;
#endif
}

// 非阻塞版本的模块化调试日志
int32_t try_logger_module_debug(uint32_t module_id, const char *fmt, ...)
{
#if __LOG_LEVEL <= LOG_LEVEL_DEBUG
    if (!is_module_debug_enabled(module_id)) {
        return 0;
    }

    va_list va;
    va_start(va, fmt);
    int32_t r = try_logger_impl(ANSI_BLUE, &missed_log_debug, fmt, va);
    va_end(va);
    return r;
#else
    (void)module_id;
    (void)fmt;
    return 0;
#endif
}