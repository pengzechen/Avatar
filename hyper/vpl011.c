#include "hyper/vpl011.h"
#include "hyper/vm.h"
#include "task/task.h"
#include "uart_pl011.h"
#include "io.h"
#include "lib/aj_string.h"
#include "thread.h"
#include "hyper/vgic.h"

/* Virtual PL011 management structures */
static vpl011_t _vpl011s[VM_NUM_MAX];
static uint32_t _vpl011_num = 0;

/* Virtual PL011 state pool (one per VM) */
static vpl011_state_t _vpl011_states[VM_NUM_MAX];
static uint32_t _vpl011_state_num = 0;

/* --------------------------------------------------------
 * ==================    初始化函数    ==================
 * -------------------------------------------------------- */

void vpl011_global_init(void)
{
    memset(_vpl011s, 0, sizeof(_vpl011s));
    memset(_vpl011_states, 0, sizeof(_vpl011_states));
    _vpl011_num = 0;
    _vpl011_state_num = 0;
    logger_info("Virtual PL011 global initialized\n");
}

vpl011_t *alloc_vpl011(void)
{
    if (_vpl011_num >= VM_NUM_MAX) {
        logger_error("No more vpl011 can be allocated!\n");
        return NULL;
    }

    vpl011_t *vpl011 = &_vpl011s[_vpl011_num++];
    memset(vpl011, 0, sizeof(vpl011_t));

    logger_info("Allocated vpl011 %d\n", _vpl011_num - 1);
    return vpl011;
}

vpl011_state_t *alloc_vpl011_state(void)
{
    if (_vpl011_state_num >= VM_NUM_MAX) {
        logger_error("No more vpl011 state can be allocated!\n");
        return NULL;
    }

    vpl011_state_t *vuart = &_vpl011_states[_vpl011_state_num++];
    vpl011_state_init(vuart);
    return vuart;
}

vpl011_state_t *get_vpl011_by_vm(struct _vm_t *vm)
{
    if (!vm || !vm->vpl011 || !vm->vpl011->state) {
        return NULL;
    }

    return vm->vpl011->state;
}

void vpl011_state_init(vpl011_state_t *vuart)
{
    memset(vuart, 0, sizeof(vpl011_state_t));

    /* Initialize registers to reset values */
    vuart->dr = 0x00;
    vuart->rsr = 0x00;
    vuart->fr = VUART_FR_TXFE | VUART_FR_RXFE;  /* TX FIFO empty, RX FIFO empty */
    vuart->ilpr = 0x00;
    vuart->ibrd = 0x00;
    vuart->fbrd = 0x00;
    vuart->lcr_h = 0x00;
    vuart->cr = 0x300;      /* TXE=1, RXE=1 (TX enabled, RX enabled) */
    vuart->ifls = 0x12;     /* Default FIFO levels */
    vuart->imsc = 0x00;     /* All interrupts masked */
    vuart->ris = 0x00;      /* No raw interrupts */
    vuart->mis = 0x00;      /* No masked interrupts */

    /* Virtual UART specific state */
    vuart->base_addr = 0x09000000;  /* Default PL011 base address */
    vuart->irq_num = 33;            /* Default PL011 IRQ */
    vuart->initialized = true;

    /* Initialize FIFO state */
    vuart->tx_count = 0;
    vuart->rx_count = 0;

    logger_info("Virtual PL011 state initialized\n");
}

/* --------------------------------------------------------
 * ==================    寄存器访问    ==================
 * -------------------------------------------------------- */

uint32_t vpl011_read(vpl011_state_t *vuart, uint32_t offset)
{
    if (!vuart || !vuart->initialized) {
        return 0;
    }
    
    switch (offset) {
    case VUART_DR:
        /* Read from RX FIFO */
        if (vuart->rx_count > 0) {
            uint8_t data = vuart->rx_fifo[0];
            /* Shift FIFO contents */
            for (uint32_t i = 0; i < vuart->rx_count - 1; i++) {
                vuart->rx_fifo[i] = vuart->rx_fifo[i + 1];
            }
            vuart->rx_count--;

            /* Update flag register */
            if (vuart->rx_count == 0) {
                vuart->fr |= VUART_FR_RXFE;
            }
            vuart->fr &= ~VUART_FR_RXFF;

            logger_debug("VUART read DR: 0x%x ('%c'), rx_count=%d\n",
                        data, (char)data, vuart->rx_count);

            /* Update interrupts after reading */
            vpl011_update_interrupts(vuart);

            return data;
        } else {
            logger_debug("VUART read DR: RX FIFO empty, returning 0\n");
            return 0;
        }
        
    case VUART_RSR:
        return vuart->rsr;
        
    case VUART_FR:
        /* Update flag register based on FIFO state */
        vuart->fr &= ~(VUART_FR_TXFE | VUART_FR_RXFE | VUART_FR_TXFF | VUART_FR_RXFF);
        if (vuart->tx_count == 0) vuart->fr |= VUART_FR_TXFE;
        if (vuart->tx_count >= 16) vuart->fr |= VUART_FR_TXFF;
        if (vuart->rx_count == 0) vuart->fr |= VUART_FR_RXFE;
        if (vuart->rx_count >= 16) vuart->fr |= VUART_FR_RXFF;
        return vuart->fr;
        
    case VUART_ILPR:
        return vuart->ilpr;
        
    case VUART_IBRD:
        return vuart->ibrd;
        
    case VUART_FBRD:
        return vuart->fbrd;
        
    case VUART_LCR_H:
        return vuart->lcr_h;
        
    case VUART_CR:
        return vuart->cr;
        
    case VUART_IFLS:
        return vuart->ifls;
        
    case VUART_IMSC:
        return vuart->imsc;
        
    case VUART_RIS:
        return vuart->ris;
        
    case VUART_MIS:
        /* Masked interrupt status = raw & mask */
        vuart->mis = vuart->ris & vuart->imsc;
        return vuart->mis;
        
    default:
        logger_warn("VUART read unknown offset: 0x%x\n", offset);
        return 0;
    }
}

void vpl011_write(vpl011_state_t *vuart, uint32_t offset, uint32_t value)
{
    if (!vuart || !vuart->initialized) {
        return;
    }
    
    switch (offset) {
    case VUART_DR:
        logger_debug("VUART write DR: 0x%x (char: '%c')\n", value, (char)(value & 0xFF));

        /* Add to TX FIFO if not full */
        if (vuart->tx_count < 16) {
            vuart->tx_fifo[vuart->tx_count++] = (uint8_t)(value & 0xFF);

            /* Update flag register */
            vuart->fr &= ~VUART_FR_TXFE;
            if (vuart->tx_count >= 16) {
                vuart->fr |= VUART_FR_TXFF;
            }

            /* Forward to physical UART immediately for now */
            uart_putchar((char)(value & 0xFF));

            /* Simulate immediate transmission - remove from TX FIFO */
            vuart->tx_count--;
            if (vuart->tx_count == 0) {
                vuart->fr |= VUART_FR_TXFE;
            }
            vuart->fr &= ~VUART_FR_TXFF;

            /* Update interrupts after writing */
            vpl011_update_interrupts(vuart);
        } else {
            logger_warn("VUART: TX FIFO full, dropping char '%c'\n", (char)(value & 0xFF));
        }
        break;
        
    case VUART_RSR:
        /* Write 1 to clear error bits */
        vuart->rsr &= ~(value & 0x0F);
        break;
        
    case VUART_ILPR:
        vuart->ilpr = value & 0xFF;
        break;
        
    case VUART_IBRD:
        vuart->ibrd = value & 0xFFFF;
        break;
        
    case VUART_FBRD:
        vuart->fbrd = value & 0x3F;
        break;
        
    case VUART_LCR_H:
        vuart->lcr_h = value & 0xFF;
        break;
        
    case VUART_CR:
        vuart->cr = value & 0xFFFF;
        logger_debug("VUART write CR: 0x%x\n", vuart->cr);
        break;
        
    case VUART_IFLS:
        vuart->ifls = value & 0x3F;
        break;
        
    case VUART_IMSC:
        vuart->imsc = value & 0x7FF;
        logger_debug("VUART write IMSC: 0x%x\n", vuart->imsc);
        /* Update interrupts when mask changes */
        vpl011_update_interrupts(vuart);
        break;
        
    case VUART_ICR:
        /* Write 1 to clear interrupt bits */
        vuart->ris &= ~(value & 0x7FF);
        logger_debug("VUART clear interrupts: 0x%x\n", value);
        /* Update interrupts after clearing */
        vpl011_update_interrupts(vuart);
        break;
        
    default:
        logger_warn("VUART write unknown offset: 0x%x, value: 0x%x\n", offset, value);
        break;
    }
}

/* --------------------------------------------------------
 * ==================    MMIO处理    ==================
 * -------------------------------------------------------- */

int32_t vpl011_mmio_handler(uint64_t gpa, uint32_t reg_num, uint32_t len,
                           uint64_t *reg_data, bool is_write)
{
    /* Get current task and its vpl011 state */
    tcb_t *current_task = (tcb_t*)read_tpidr_el2();
    if (!current_task) {
        logger_error("vpl011_mmio_handler: no current task\n");
        return 0;
    }

    vpl011_state_t *vuart = get_vpl011_by_vm(current_task->curr_vm);
    if (!vuart) {
        logger_error("vpl011_mmio_handler: no vpl011 state for current VM\n");
        return 0;
    }

    /* Calculate register offset from base address */
    uint32_t offset = gpa - vuart->base_addr;

    if (is_write) {
        /* MMIO Write */
        uint32_t value = (uint32_t)(*reg_data);
        vpl011_write(vuart, offset, value);
        logger_debug("VUART MMIO write: offset=0x%x, value=0x%x\n", offset, value);
    } else {
        /* MMIO Read */
        uint32_t value = vpl011_read(vuart, offset);
        *reg_data = value;
        logger_debug("VUART MMIO read: offset=0x%x, value=0x%x\n", offset, value);
    }

    return 1;  /* Successfully handled */
}

/* --------------------------------------------------------
 * ==================    中断处理    ==================
 * -------------------------------------------------------- */

void vpl011_update_interrupts(vpl011_state_t *vuart)
{
    if (!vuart || !vuart->initialized) {
        return;
    }

    /* Update raw interrupt status based on FIFO state and conditions */
    vuart->ris = 0;

    /* RX interrupt: FIFO not empty and RX enabled */
    if (vuart->rx_count > 0 && (vuart->cr & (1 << 9))) {
        vuart->ris |= (1 << 4);  /* RXIM */
    }

    /* TX interrupt: FIFO not full and TX enabled */
    if (vuart->tx_count < 16 && (vuart->cr & (1 << 8))) {
        vuart->ris |= (1 << 5);  /* TXIM */
    }

    /* Update masked interrupt status */
    vuart->mis = vuart->ris & vuart->imsc;

    /* Inject interrupt to guest if any masked interrupts are pending */
    if (vuart->mis != 0) {
        /* Find the VM that owns this UART */
        for (uint32_t i = 0; i < _vpl011_num; i++) {
            if (_vpl011s[i].state == vuart && _vpl011s[i].vm) {
                /* Get the primary vCPU of this VM */
                tcb_t *primary_vcpu = _vpl011s[i].vm->primary_vcpu;
                if (primary_vcpu) {
                    logger_info("VUART: Injecting SPI 33 to VM %d\n", _vpl011s[i].vm->vm_id);
                    vgic_inject_spi(primary_vcpu, 33);  /* PL011 IRQ is 33 */
                }
                break;
            }
        }
    }
}

void vpl011_inject_rx_char(vpl011_state_t *vuart, char c)
{
    if (!vuart || !vuart->initialized) {
        return;
    }

    /* Add character to RX FIFO if not full */
    if (vuart->rx_count < 16) {
        vuart->rx_fifo[vuart->rx_count++] = (uint8_t)c;

        /* Update flag register - RX FIFO no longer empty */
        vuart->fr &= ~VUART_FR_RXFE;
        if (vuart->rx_count >= 16) {
            vuart->fr |= VUART_FR_RXFF;
        }

        logger_debug("VUART: Injected char '%c' (0x%02x), rx_count=%d\n",
                    c, (unsigned char)c, vuart->rx_count);

        /* Update interrupts */
        vpl011_update_interrupts(vuart);
    } else {
        logger_warn("VUART: RX FIFO full, dropping char '%c'\n", c);
    }
}

void vpl011_handle_physical_uart_rx(char c)
{
    /* Inject the character to all active VMs' virtual UARTs */
    for (uint32_t i = 0; i < _vpl011_num; i++) {
        if (_vpl011s[i].state && _vpl011s[i].vm) {
            vpl011_inject_rx_char(_vpl011s[i].state, c);
        }
    }
}
