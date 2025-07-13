#include <uart_pl011.h>
#include <spinlock.h>
#include <io.h>

// 环形缓冲区大小
#define BUFFER_SIZE 1024

// 接收环形缓冲区
char rx_buffer[BUFFER_SIZE];
int rx_read_idx = 0;
int rx_write_idx = 0;

// 发送环形缓冲区
char tx_buffer[BUFFER_SIZE];
int tx_read_idx = 0;
int tx_write_idx = 0;

static spinlock_t lock;

#define UART_VECTOR 33

void uart_advance_init()
{
    // 禁用 UART
    UART0_CR = 0x0;
    spinlock_init(&lock);

    // 设置波特率，例如 115200
    // 波特率计算公式：
    // 整数部分 = UARTCLK / (16 * 波特率)
    // 小数部分 = 整数部分的小数部分 * 64 + 0.5
    // 假设 UARTCLK 为 24MHz
    uint32_t uartclk = 24000000;
    uint32_t baudrate = 115200;
    uint32_t ibrd = uartclk / (16 * baudrate);
    uint32_t fbrd = (uartclk % (16 * baudrate)) * 4 / baudrate;

    UART0_IBRD = ibrd;
    UART0_FBRD = fbrd;

    // 配置线路控制寄存器: 8 bits, no parity, one stop bit
    UART0_LCRH = (1 << 5) | (1 << 6);

    UART0_IMSC = 0;
    // 使能RX和TX中断
    UART0_IMSC = (0b11 << 4);

    // 启用 UART，TX 和 RX
    UART0_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

char uart_advance_getc(void)
{
    while (rx_read_idx == rx_write_idx)
    {
        // 等待数据发送到缓冲区
    }
    char c = rx_buffer[rx_read_idx];
    rx_read_idx = (rx_read_idx + 1) % BUFFER_SIZE;
    return c;
}

void uart_advance_putc(char c)
{
    int next_write_idx = (tx_write_idx + 1) % BUFFER_SIZE;
    while (next_write_idx == tx_read_idx)
    {
        // 等待缓冲区有空闲空间
    }
    tx_buffer[tx_write_idx] = c;
    tx_write_idx = next_write_idx;
    UART0_IMSC |= 0x20; // 使能TX中断
}

// 中断处理程序
void uart_interrupt_handler(uint64_t *)
{
    // 处理接收中断
    if (UART0_MIS & 0x10)
    { // RX中断
        while (!(UART0_FR & 0x10))
        { // RX FIFO不为空
            char c = UART0_DR;
            int next_write_idx = (rx_write_idx + 1) % BUFFER_SIZE;
            if (next_write_idx != rx_read_idx)
            {
                rx_buffer[rx_write_idx] = c;
                rx_write_idx = next_write_idx;
            }
        }
        UART0_ICR = 0x10; // 清除RX中断
    }

    // 处理发送中断
    if (UART0_MIS & 0x20)
    { // TX中断
        while (!(UART0_FR & 0x20) && (tx_read_idx != tx_write_idx))
        { // TX FIFO不满
            UART0_DR = tx_buffer[tx_read_idx];
            tx_read_idx = (tx_read_idx + 1) % BUFFER_SIZE;
        }
        if (tx_read_idx == tx_write_idx)
        {
            UART0_IMSC &= ~0x20; // 禁用TX中断
        }
        UART0_ICR = 0x20; // 清除TX中断
    }
}