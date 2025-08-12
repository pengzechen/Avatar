#include "uart_pl011.h"
#include "spinlock.h"
#include "io.h"
#include "mmio.h"

#include "gic.h"
#include "exception.h"
#include "vmm/vpl011.h"

// Circular buffer for transmit data
#define UART_TX_BUFFER_SIZE 32
#define UART_RX_BUFFER_SIZE 32

typedef struct
{
    char buffer[UART_TX_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    spinlock_t lock;
} uart_buffer_t;

static uart_buffer_t tx_buffer = {0};
static uart_buffer_t rx_buffer = {0};
static volatile bool uart_initialized = false;

// Buffer operations
static bool buffer_is_empty(uart_buffer_t *buf)
{
    return buf->count == 0;
}

static bool buffer_is_full(uart_buffer_t *buf)
{
    return buf->count >= UART_TX_BUFFER_SIZE;
}

static bool buffer_put(uart_buffer_t *buf, char c)
{
    if (buffer_is_full(buf))
    {
        return false;
    }

    buf->buffer[buf->head] = c;
    buf->head = (buf->head + 1) % UART_TX_BUFFER_SIZE;
    buf->count++;
    return true;
}

static bool buffer_get(uart_buffer_t *buf, char *c)
{
    if (buffer_is_empty(buf))
    {
        return false;
    }

    *c = buf->buffer[buf->tail];
    buf->tail = (buf->tail + 1) % UART_TX_BUFFER_SIZE;
    buf->count--;
    return true;
}

// UART hardware operations
static void uart_enable_tx_interrupt(void)
{
    uint32_t imsc = mmio_read32((void *)UART_IMSC);
    imsc |= UART_INT_TX;
    mmio_write32(imsc, (void *)UART_IMSC);
}

static void uart_disable_tx_interrupt(void)
{
    uint32_t imsc = mmio_read32((void *)UART_IMSC);
    imsc &= ~UART_INT_TX;
    mmio_write32(imsc, (void *)UART_IMSC);
}

static void uart_enable_rx_interrupt(void)
{
    uint32_t imsc = mmio_read32((void *)UART_IMSC);
    imsc |= (UART_INT_RX | UART_INT_RT);
    mmio_write32(imsc, (void *)UART_IMSC);
}

static bool uart_tx_fifo_full(void)
{
    return (mmio_read32((void *)UART_FR) & UART_FR_TXFF) != 0;
}

static bool uart_rx_fifo_empty(void)
{
    return (mmio_read32((void *)UART_FR) & UART_FR_RXFE) != 0;
}

// UART interrupt handler
void uart_interrupt_handler(uint64_t *stack_pointer)
{

    uint32_t mis = mmio_read32((void *)UART_MIS);

    // Handle transmit interrupt
    if (mis & UART_INT_TX)
    {
        spin_lock(&tx_buffer.lock);

        // Send as many characters as possible
        while (!uart_tx_fifo_full() && !buffer_is_empty(&tx_buffer))
        {
            char c;
            if (buffer_get(&tx_buffer, &c))
            {
                mmio_write32((uint32_t)c, (void *)UART_DR);
            }
        }

        // If buffer is empty, disable TX interrupt
        if (buffer_is_empty(&tx_buffer))
        {
            uart_disable_tx_interrupt();
        }

        spin_unlock(&tx_buffer.lock);

        // Clear TX interrupt
        mmio_write32(UART_INT_TX, (void *)UART_ICR);
    }

    // Handle receive interrupt
    if (mis & (UART_INT_RX | UART_INT_RT))
    {
        spin_lock(&rx_buffer.lock);

        // Read all available characters
        while (!uart_rx_fifo_empty())
        {
            char c = (char)mmio_read32((void *)UART_DR);
            if (!buffer_is_full(&rx_buffer))
            {
                buffer_put(&rx_buffer, c);
            }
            // If buffer is full, we drop the character
            
            // 测试使用 不要常开着
            // logger_info("uart_interrupt_handler\n");
            
            /* Forward character to virtual UARTs */
            vpl011_handle_physical_uart_rx(c);
        }

        spin_unlock(&rx_buffer.lock);

        // Clear RX interrupts
        mmio_write32(UART_INT_RX | UART_INT_RT, (void *)UART_ICR);
    }
}

// Initialize UART with interrupt support
void uart_init(void)
{
    if (uart_initialized)
    {
        return;
    }

    // Initialize buffers
    spinlock_init(&tx_buffer.lock);
    spinlock_init(&rx_buffer.lock);
    tx_buffer.head = tx_buffer.tail = tx_buffer.count = 0;
    rx_buffer.head = rx_buffer.tail = rx_buffer.count = 0;

    // Disable UART first
    mmio_write32(0, (void *)UART_CR);

    // Clear all interrupts
    mmio_write32(0x7FF, (void *)UART_ICR);

    // Configure UART (115200 baud, 8N1)
    // For 24MHz clock: IBRD = 24000000 / (16 * 115200) = 13
    // FBRD = (0.0208 * 64) + 0.5 = 1
    mmio_write32(13, (void *)UART_IBRD);
    mmio_write32(1, (void *)UART_FBRD);

    // 8 bits, no parity, 1 stop bit, enable FIFOs
    mmio_write32((3 << 5) | (1 << 4), (void *)UART_LCR_H);

    // Enable UART, TX, RX
    mmio_write32(UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE, (void *)UART_CR);

    // Enable RX interrupts (TX interrupts enabled on demand)
    uart_enable_rx_interrupt();

    // Install interrupt handler
    irq_install(UART_IRQ, uart_interrupt_handler);

    // Enable UART interrupt in GIC
    gic_enable_int(UART_IRQ, 1);
    gic_set_target(33, 0b00000001);
    gic_set_ipriority(UART_IRQ, 0);

    uart_initialized = true;

    logger_info("UART interrupt driver initialized\n");
}

// Non-blocking character output
bool uart_putchar_nb(char c)
{
    if (!uart_initialized)
    {
        return false;
    }

    spin_lock(&tx_buffer.lock);

    bool success = false;

    // Try to put directly to FIFO if buffer is empty and FIFO not full
    if (buffer_is_empty(&tx_buffer) && !uart_tx_fifo_full())
    {
        mmio_write32((uint32_t)c, (void *)UART_DR);
        success = true;
    }
    else
    {
        // Put to buffer
        success = buffer_put(&tx_buffer, c);
        if (success)
        {
            // Enable TX interrupt to drain the buffer
            uart_enable_tx_interrupt();
        }
    }

    spin_unlock(&tx_buffer.lock);
    return success;
}

// Blocking character output (with timeout)
void uart_putchar(char c)
{
    if (!uart_initialized)
    {
        // Fallback to direct write if not initialized
        volatile unsigned int *const UART0DR = (unsigned int *)UART_DR;
        *UART0DR = (unsigned int)c;
        return;
    }

    // Try non-blocking first
    if (uart_putchar_nb(c))
    {
        return;
    }

    // If buffer is full, wait a bit and retry
    int timeout = 10000;
    while (timeout-- > 0)
    {
        if (uart_putchar_nb(c))
        {
            return;
        }
        // Small delay
        for (int i = 0; i < 100; i++)
        {
            asm volatile("nop");
        }
    }

    // If still failed, drop the character
    logger_warn("UART TX buffer full, dropping character\n");
}

// Non-blocking character input
bool uart_getchar_nb(char *c)
{
    if (!uart_initialized)
    {
        return false;
    }

    spin_lock(&rx_buffer.lock);
    bool success = buffer_get(&rx_buffer, c);
    spin_unlock(&rx_buffer.lock);

    return success;
}

// Check if RX data is available
bool uart_rx_available(void)
{
    if (!uart_initialized)
    {
        return false;
    }

    spin_lock(&rx_buffer.lock);
    bool available = !buffer_is_empty(&rx_buffer);
    spin_unlock(&rx_buffer.lock);

    return available;
}

// Get TX buffer usage
uint32_t uart_tx_buffer_usage(void)
{
    if (!uart_initialized)
    {
        return 0;
    }

    spin_lock(&tx_buffer.lock);
    uint32_t usage = tx_buffer.count;
    spin_unlock(&tx_buffer.lock);

    return usage;
}
