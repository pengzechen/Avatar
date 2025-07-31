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

#define BINSTR_SZ (sizeof(uint32_t) * 8 + sizeof(uint32_t) * 2)

#define BUFSZ 512

#define ANSI_RED     "\x1b[31m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_RESET   "\x1b[0m"

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

int32_t logger(const char *fmt, ...)
{
    va_list va;
    char buf[BUFSZ];
    int32_t r;

    va_start(va, fmt);
    r = my_vsnprintf(buf, sizeof buf, fmt, va);
    va_end(va);

    uart_putstr(buf);

    return r;
}

int32_t logger_warn(const char *fmt, ...)
{
    va_list va;
    char buf[BUFSZ];
    int32_t r;

    va_start(va, fmt);
    r = my_vsnprintf(buf, sizeof buf, fmt, va);
    va_end(va);

    uart_putstr(ANSI_YELLOW);
    uart_putstr(buf);
    uart_putstr(ANSI_RESET);

    return r;
}

int32_t logger_error(const char *fmt, ...)
{
    va_list va;
    char buf[BUFSZ];
    int32_t r;

    va_start(va, fmt);
    r = my_vsnprintf(buf, sizeof buf, fmt, va);
    va_end(va);

    uart_putstr(ANSI_RED);
    uart_putstr(buf);
    uart_putstr(ANSI_RESET);

    return r;
}

int32_t logger_info(const char *fmt, ...)
{
    va_list va;
    char buf[BUFSZ];
    int32_t r;

    va_start(va, fmt);
    r = my_vsnprintf(buf, sizeof buf, fmt, va);
    va_end(va);

    uart_putstr(ANSI_GREEN);
    uart_putstr(buf);
    uart_putstr(ANSI_RESET);

    return r;
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