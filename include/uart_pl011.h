

#ifndef __PL011_H__
#define __PL011_H__

#include "aj_types.h"
#include "os_cfg.h"

// UART PL011 register definitions
#define UART_BASE_ADDR  0x9000000
// UART interrupt number (SPI 1 = GIC interrupt 33)
#define UART_IRQ     33

#define UART_DR         (UART_BASE_ADDR + 0x000)  // Data Register
#define UART_RSR        (UART_BASE_ADDR + 0x004)  // Receive Status Register
#define UART_FR         (UART_BASE_ADDR + 0x018)  // Flag Register
#define UART_ILPR       (UART_BASE_ADDR + 0x020)  // IrDA Low-Power Counter Register
#define UART_IBRD       (UART_BASE_ADDR + 0x024)  // Integer Baud Rate Register
#define UART_FBRD       (UART_BASE_ADDR + 0x028)  // Fractional Baud Rate Register
#define UART_LCR_H      (UART_BASE_ADDR + 0x02C)  // Line Control Register
#define UART_CR         (UART_BASE_ADDR + 0x030)  // Control Register
#define UART_IFLS       (UART_BASE_ADDR + 0x034)  // Interrupt FIFO Level Select Register
#define UART_IMSC       (UART_BASE_ADDR + 0x038)  // Interrupt Mask Set/Clear Register
#define UART_RIS        (UART_BASE_ADDR + 0x03C)  // Raw Interrupt Status Register
#define UART_MIS        (UART_BASE_ADDR + 0x040)  // Masked Interrupt Status Register
#define UART_ICR        (UART_BASE_ADDR + 0x044)  // Interrupt Clear Register

// UART Flag Register bits
#define UART_FR_TXFF    (1 << 5)  // Transmit FIFO full
#define UART_FR_RXFE    (1 << 4)  // Receive FIFO empty
#define UART_FR_BUSY    (1 << 3)  // UART busy

// UART Interrupt bits
#define UART_INT_TX     (1 << 5)  // Transmit interrupt
#define UART_INT_RX     (1 << 4)  // Receive interrupt
#define UART_INT_RT     (1 << 6)  // Receive timeout interrupt

// UART Control Register bits
#define UART_CR_UARTEN  (1 << 0)  // UART enable
#define UART_CR_TXE     (1 << 8)  // Transmit enable
#define UART_CR_RXE     (1 << 9)  // Receive enable


// Initialize UART with interrupt support
void uart_init(void);

// Character output functions
void uart_putchar(char c);           // Blocking output
bool uart_putchar_nb(char c);        // Non-blocking output
void uart_putstr(const char *str);   // String output

// Character input functions  
bool uart_getchar_nb(char *c);       // Non-blocking input
bool uart_rx_available(void);        // Check if RX data available

// Buffer status
uint32_t uart_tx_buffer_usage(void); // Get TX buffer usage

// Interrupt handler (internal use)
void uart_interrupt_handler(uint64_t *stack_pointer);

#endif // __PL011_H__