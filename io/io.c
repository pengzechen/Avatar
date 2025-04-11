
#include <uart_pl011.h>
#include <spinlock.h>
#include <io.h>
#include <gic.h>
#include <exception.h>

struct uart_ops_t
{
    void (*uart_putc)(char);
    char (*uart_getc)(void);
};

static const struct uart_ops_t *uart_op = NULL;

static const struct uart_ops_t early_ops = {
    .uart_putc = uart_early_putc,
    .uart_getc = uart_early_getc,
};

static const struct uart_ops_t advance_ops = {
    .uart_putc = uart_advance_putc,
    .uart_getc = uart_advance_getc,
};

// 初期使用的串口
void io_early_init()
{
    uart_early_init();
    uart_op = &early_ops;
}

void io_init()
{
    extern void uart_interrupt_handler(uint64_t *);
    irq_install(33, uart_interrupt_handler);
    gic_enable_int(33, 0);

    if (gic_get_enable(33))
    {
        printf("uart enabled successfully ...\n");
    }

    uart_advance_init();
    uart_op = &advance_ops;
}

char getc()
{
    return uart_op->uart_getc();
}

void putc(char c)
{
    uart_op->uart_putc(c);
}

void uart_putstr(const char *str)
{
    if (uart_op == NULL)
        uart_op = &early_ops;
    while (*str)
    {
        uart_op->uart_putc(*str++);  // putchar()
    }
}

void print_info(const char *info)
{
    // ANSI 转义序列: "\033[32m" 设置绿色前景色, "\033[0m" 重置颜色
    // printf("\033[32m%s\033[0m", info);
    printf("%s", info);
}

void print_warn(const char *info)
{
    // ANSI 转义序列: "\033[32m" 设置绿色前景色, "\033[0m" 重置颜色
    // printf("\033[33m%s\033[0m", info);
    printf("%s", info);
}