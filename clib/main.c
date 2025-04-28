
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <reent.h>
#include <string.h>

// typedef unsigned long uint32_t;
#define UART0_BASE 0x9000000
#define UART0_FR (*(volatile uint32_t *)(UART0_BASE + 0x18))   // 标志寄存器
#define UART0_DR (*(volatile uint32_t *)(UART0_BASE + 0x00))   // 数据寄存器
#define UART0_IBRD (*(volatile uint32_t *)(UART0_BASE + 0x24)) // 整数波特率除数寄存器
#define UART0_FBRD (*(volatile uint32_t *)(UART0_BASE + 0x28)) // 小数波特率除数寄存器
#define UART0_LCRH (*(volatile uint32_t *)(UART0_BASE + 0x2C)) // 线路控制寄存器
#define UART0_CR (*(volatile uint32_t *)(UART0_BASE + 0x30))   // 控制寄存器

void uart_early_init()
{
    UART0_CR = 0x0;
    uint32_t uartclk = 24000000;
    uint32_t baudrate = 115200;
    uint32_t ibrd = uartclk / (16 * baudrate);
    uint32_t fbrd = (uartclk % (16 * baudrate)) * 4 / baudrate;
    UART0_IBRD = ibrd;
    UART0_FBRD = fbrd;
    UART0_LCRH = (1 << 5) | (1 << 6);
    UART0_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

bool uart_early_putc(char c)
{

    // 等待发送 FIFO 不为满
    while (UART0_FR & (1 << 5))
        ;
    UART0_DR = c;
    return true;
}

char uart_early_getc()
{
    // 等待接收 FIFO 不为空
    while (UART0_FR & (1 << 4))
        ;
    return (char)UART0_DR;
}

extern int _end;

static unsigned char *heap = NULL;

uint8_t kstack[8192] __attribute__((section(".bss.kstack")));




int _open(const char *, int, ...) {
    return 0;
}

int _close(int file)
{
    return -1;
}

int _write(int file, char *ptr, int len)
{
    int written = 0;

    if ((file != 1) && (file != 2) && (file != 3))
    {
        return -1;
    }

    for (; len != 0; --len)
    {
        uart_early_putc(*ptr);
        ptr++;

        ++written;
    }
    return written;
}

int _read(int file, char *ptr, int len)
{
    int read = 0;

    if (file != 0)
    {
        return -1;
    }

    for (; len > 0; --len)
    {
        *ptr = uart_early_getc();
        ptr++;
        read++;
    }
    return read;
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;

    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}


void *_sbrk(int incr) 
{
    unsigned char *prev_heap;
    if (heap == NULL)
    {
        heap = (unsigned char *)&_end;
    }
    prev_heap = heap;
    heap += incr;
    return prev_heap;
}

void _exit(int status)
{
    __asm("brk #0");
    while (1)
        ;
}

void _kill(int pid, int sig)
{
    return;
}

int _getpid(void)
{
    return -1;
}



struct _reent my_reent_data __attribute__((aligned(64)));

void init_reent_data()
{
    _REENT_INIT_PTR(&my_reent_data);
    _impure_ptr = &my_reent_data;
}

char res[10] = {0};

int main()
{
    int *a = (int *)malloc(sizeof(int));
    init_reent_data();
    uart_early_init();
    setbuf(stdin, NULL);

    printf("hello world\n");
    int x = fwrite("r\n", 1, 2, stdout); // 每个字符 1 字节，写入 2 个字符
    printf("write %d bytes\n", x);

    fread(&res, 1, 10, stdin);
    printf("res: %s\n", res);

    return 0;
}

