
#include "uart_pl011.h"
#include "spinlock.h"
#include "io.h"
#include "gic.h"


struct uart_ops_t
{
    void (*uart_putc)(char);
};

static const struct uart_ops_t *uart_op = NULL;

static const struct uart_ops_t early_ops = {
    .uart_putc = uart_early_putc,
};

static const struct uart_ops_t advance_ops = {
    .uart_putc = uart_putchar,
};

// 初期使用的串口
void io_early_init()
{
    uart_early_init();
    uart_op = &early_ops;
}

void io_init()
{
    uart_init();
    uart_op = &advance_ops;
}

char getc()
{
    return uart_early_getc();
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
        uart_op->uart_putc(*str++); // putchar()
    }
}

