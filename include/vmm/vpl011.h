#ifndef __VPL011_H__
#define __VPL011_H__

#include "aj_types.h"
#include "task/task.h"

typedef struct _tcb_t tcb_t;

/* Virtual PL011 UART registers - same layout as physical PL011 */
#define VUART_DR        0x000   /* Data Register */
#define VUART_RSR       0x004   /* Receive Status Register */
#define VUART_FR        0x018   /* Flag Register */
#define VUART_ILPR      0x020   /* IrDA Low-Power Counter Register */
#define VUART_IBRD      0x024   /* Integer Baud Rate Register */
#define VUART_FBRD      0x028   /* Fractional Baud Rate Register */
#define VUART_LCR_H     0x02C   /* Line Control Register */
#define VUART_CR        0x030   /* Control Register */
#define VUART_IFLS      0x034   /* Interrupt FIFO Level Select Register */
#define VUART_IMSC      0x038   /* Interrupt Mask Set/Clear Register */
#define VUART_RIS       0x03C   /* Raw Interrupt Status Register */
#define VUART_MIS       0x040   /* Masked Interrupt Status Register */
#define VUART_ICR       0x044   /* Interrupt Clear Register */

/* PL011 Identification Registers */
#define VUART_PERIPHID0 0xFE0   /* Peripheral ID Register 0 */
#define VUART_PERIPHID1 0xFE4   /* Peripheral ID Register 1 */
#define VUART_PERIPHID2 0xFE8   /* Peripheral ID Register 2 */
#define VUART_PERIPHID3 0xFEC   /* Peripheral ID Register 3 */
#define VUART_PCELLID0  0xFF0   /* PrimeCell ID Register 0 */
#define VUART_PCELLID1  0xFF4   /* PrimeCell ID Register 1 */
#define VUART_PCELLID2  0xFF8   /* PrimeCell ID Register 2 */
#define VUART_PCELLID3  0xFFC   /* PrimeCell ID Register 3 */

/* PL011 Identification Register Values */
#define VUART_PERIPHID0_VAL 0x11    /* Part number [7:0] */
#define VUART_PERIPHID1_VAL 0x10    /* Part number [11:8] and Designer [3:0] */
#define VUART_PERIPHID2_VAL 0x14    /* Revision [7:4] and Designer [7:4] */
#define VUART_PERIPHID3_VAL 0x00    /* Configuration */
#define VUART_PCELLID0_VAL  0x0D    /* PrimeCell ID */
#define VUART_PCELLID1_VAL  0xF0    /* PrimeCell ID */
#define VUART_PCELLID2_VAL  0x05    /* PrimeCell ID */
#define VUART_PCELLID3_VAL  0xB1    /* PrimeCell ID */

/* PL011 Flag Register bits */
#define VUART_FR_TXFE   (1 << 7)  /* Transmit FIFO empty */
#define VUART_FR_RXFF   (1 << 6)  /* Receive FIFO full */
#define VUART_FR_TXFF   (1 << 5)  /* Transmit FIFO full */
#define VUART_FR_RXFE   (1 << 4)  /* Receive FIFO empty */
#define VUART_FR_BUSY   (1 << 3)  /* UART busy */

/* Virtual PL011 UART state (per VM) */
typedef struct _vpl011_state_t
{
    /* Register values */
    uint32_t dr;        /* Data register */
    uint32_t rsr;       /* Receive status register */
    uint32_t fr;        /* Flag register */
    uint32_t ilpr;      /* IrDA low-power register */
    uint32_t ibrd;      /* Integer baud rate register */
    uint32_t fbrd;      /* Fractional baud rate register */
    uint32_t lcr_h;     /* Line control register */
    uint32_t cr;        /* Control register */
    uint32_t ifls;      /* Interrupt FIFO level select register */
    uint32_t imsc;      /* Interrupt mask set/clear register */
    uint32_t ris;       /* Raw interrupt status register */
    uint32_t mis;       /* Masked interrupt status register */

    /* Virtual UART specific state */
    uint32_t base_addr; /* Base address for this virtual UART */
    uint32_t irq_num;   /* Virtual IRQ number */
    bool initialized;   /* Initialization state */

    /* FIFO simulation (simple for now) */
    uint8_t tx_fifo[16];
    uint8_t rx_fifo[16];
    uint32_t tx_count;
    uint32_t rx_count;
} vpl011_state_t;

/* Forward declaration */
struct _vm_t;

/* Virtual PL011 management structure (per VM) */
typedef struct _vpl011_t
{
    struct _vm_t *vm;        /* Associated virtual machine */
    vpl011_state_t *state;   /* UART state (one per VM) */
} vpl011_t;

/* Function declarations */
void vpl011_global_init(void);
vpl011_t *alloc_vpl011(void);
vpl011_state_t *alloc_vpl011_state(void);
vpl011_state_t *get_vpl011_by_vm(struct _vm_t *vm);

void vpl011_state_init(vpl011_state_t *vuart);
uint32_t vpl011_read(vpl011_state_t *vuart, uint32_t offset);
void vpl011_write(vpl011_state_t *vuart, uint32_t offset, uint32_t value);

/* MMIO handler for stage2 page fault */
int32_t vpl011_mmio_handler(uint64_t gpa, uint32_t reg_num, uint32_t len,
                           uint64_t *reg_data, bool is_write);

/* Input/Output functions with interrupt support */
void vpl011_inject_rx_char(vpl011_state_t *vuart, char c);
void vpl011_update_interrupts(vpl011_state_t *vuart);
void vpl011_handle_physical_uart_rx(char c);

#endif /* __VPL011_H__ */
