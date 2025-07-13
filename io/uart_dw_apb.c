// Definitions for snps,dw-apb-uart serial driver.
// Uart snps,dw-apb-uart driver in C for BST A1000b FADA board.

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    volatile uint32_t rbr;   // 0x00: Get or Put Register
    volatile uint32_t ier;   // 0x04: Interrupt Enable Register
    volatile uint32_t fcr;   // 0x08: FIFO Control Register
    volatile uint32_t lcr;   // 0x0c: Line Control Register
    volatile uint32_t mcr;   // 0x10: Modem Control Register
    volatile uint32_t lsr;   // 0x14: Line Status Register
    volatile uint32_t msr;   // 0x18: Modem Status Register
    volatile uint32_t scr;   // 0x1c: Scratch Register
    volatile uint32_t lpdll; // 0x20: Low Power Divisor Latch (Low)
    volatile uint32_t _reserved0;
    volatile uint32_t usr; // 0x7c: Uart Status Register
    volatile uint32_t _reserved1;
    volatile uint32_t dlf; // 0xc0: Divisor Latch Fraction
} DW8250Regs;

typedef struct
{
    uintptr_t base_vaddr;
} DW8250;

#define UART_SRC_CLK 25000000
#define BST_UART_DLF_LEN 6
#define BAUDRATE 115200

static inline DW8250Regs *regs(DW8250 *uart)
{
    return (DW8250Regs *)uart->base_vaddr;
}

uint32_t get_baud_divider(uint32_t baudrate)
{
    return (UART_SRC_CLK << (BST_UART_DLF_LEN - 4)) / baudrate;
}

void DW8250_init(DW8250 *uart)
{

    uint32_t divider = get_baud_divider(BAUDRATE);

    // Waiting to be no USR_BUSY.
    while (regs(uart)->usr & 0b1)
    {
    }

    // bst_serial_hw_init_clk_rst

    /* Disable interrupts and Enable FIFOs */
    regs(uart)->ier = 0;
    regs(uart)->fcr = 1;

    /* Disable flow control */
    regs(uart)->mcr = 0;

    /* Clear MCR_RTS */
    regs(uart)->mcr |= (1 << 1);

    /* Enable access DLL & DLH. Set LCR_DLAB */
    regs(uart)->lcr |= (1 << 7);

    /* Set baud rate. Set DLL, DLH, DLF */
    regs(uart)->rbr = (divider >> BST_UART_DLF_LEN) & 0xff;
    regs(uart)->ier = (divider >> (BST_UART_DLF_LEN + 8)) & 0xff;
    regs(uart)->dlf = divider & ((1 << BST_UART_DLF_LEN) - 1);

    /* Clear DLAB bit */
    regs(uart)->lcr &= ~(1 << 7);

    /* Set data length to 8 bit, 1 stop bit, no parity. Set LCR_WLS1 | LCR_WLS0 */
    regs(uart)->lcr |= 0b11;
}

void DW8250_putchar(DW8250 *uart, uint8_t c)
{
    // Check LSR_TEMT
    // Wait for last character to go.
    while (!(regs(uart)->lsr & (1 << 6)))
    {
    }
    regs(uart)->rbr = c;
}

uint8_t DW8250_getchar(DW8250 *uart)
{
    // Check LSR_DR
    // Wait for a character to arrive.
    if (regs(uart)->lsr & 0b1)
    {
        return regs(uart)->rbr & 0xff;
    }
    return 0;
}

void DW8250_set_ier(DW8250 *uart, bool enable)
{
    if (enable)
    {
        // Enable interrupts
        regs(uart)->ier = 1;
    }
    else
    {
        // Disable interrupts
        regs(uart)->ier = 0;
    }
}
