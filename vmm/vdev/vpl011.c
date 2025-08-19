/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file vpl011.c
 * @brief Implementation of vpl011.c
 * @author Avatar Project Team
 * @date 2024
 */

#include "vmm/vpl011.h"
#include "vmm/vm.h"
#include "task/task.h"
#include "uart_pl011.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "thread.h"
#include "vmm/vgic.h"
#include "timer.h"

/* Virtual PL011 management structures */
static vpl011_t _vpl011s[VM_NUM_MAX];
static uint32_t _vpl011_num = 0;

/* Virtual PL011 state pool (one per VM) */
static vpl011_state_t _vpl011_states[VM_NUM_MAX];
static uint32_t       _vpl011_state_num = 0;

/* VM Console Switching */
static uint32_t current_console_vm        = 0; /* Currently active VM for console I/O */
static bool     console_switching_enabled = true;
static bool     in_escape_sequence        = false;

/* String matching buffer for timestamp detection */
#define MATCH_BUFFER_SIZE 256
#define TARGET_STRING     "Run /init as init process"
static char     match_buffer[MATCH_BUFFER_SIZE];
static uint32_t match_buffer_pos = 0;

/* --------------------------------------------------------
 * ==================    函数声明    ==================
 * -------------------------------------------------------- */

static void
vpl011_handle_hypervisor_command(char c);
static void
vpl011_execute_hypervisor_command(const char *cmd);
static uint32_t
find_vm_id_by_vuart(vpl011_state_t *vuart);
static void
vpl011_output_char_with_prefix(vpl011_state_t *vuart, char c);

/* --------------------------------------------------------
 * ==================    辅助函数    ==================
 * -------------------------------------------------------- */

/* 格式化时间戳为 [   37.268662] 格式 */
static void
format_timestamp(char *buffer, size_t buffer_size)
{
    uint64_t current_ticks = read_cntpct_el0();
    uint64_t frequency     = read_cntfrq_el0();

    if (frequency == 0) {
        my_snprintf(buffer, buffer_size, "[    0.000000] ");
        return;
    }

    /* 计算秒和微秒 */
    uint64_t seconds         = current_ticks / frequency;
    uint64_t remaining_ticks = current_ticks % frequency;
    uint64_t microseconds    = (remaining_ticks * 1000000) / frequency;

    /* 格式化为 [   37.268662] 格式 */
    my_snprintf(buffer, buffer_size, "[%4llu.%06llu] ", seconds, microseconds);
}

/* 检查字符串匹配并输出时间戳 */
static void
check_string_match_and_output_timestamp(char c)
{
    /* 将字符添加到匹配缓冲区 */
    if (match_buffer_pos < MATCH_BUFFER_SIZE - 1) {
        match_buffer[match_buffer_pos++] = c;
        match_buffer[match_buffer_pos]   = '\0';
    } else {
        /* 缓冲区满了，移动内容 */
        memmove(match_buffer, match_buffer + 1, MATCH_BUFFER_SIZE - 2);
        match_buffer[MATCH_BUFFER_SIZE - 2] = c;
        match_buffer[MATCH_BUFFER_SIZE - 1] = '\0';
        match_buffer_pos                    = MATCH_BUFFER_SIZE - 1;
    }

    /* 检查是否匹配目标字符串 */
    if (strstr(match_buffer, TARGET_STRING) != NULL) {
        char timestamp[32];
        format_timestamp(timestamp, sizeof(timestamp));

        /* 输出时间戳到物理UART */
        uart_putstr("\n");
        uart_putstr(timestamp);
        uart_putstr("Detected target string: ");
        uart_putstr(TARGET_STRING);
        uart_putstr("\n");

        /* 清空匹配缓冲区 */
        match_buffer_pos = 0;
        match_buffer[0]  = '\0';
    }
}

/* --------------------------------------------------------
 * ==================    初始化函数    ==================
 * -------------------------------------------------------- */

void
vpl011_global_init(void)
{
    memset(_vpl011s, 0, sizeof(_vpl011s));
    memset(_vpl011_states, 0, sizeof(_vpl011_states));
    _vpl011_num       = 0;
    _vpl011_state_num = 0;

    /* Initialize console switching */
    current_console_vm        = 0; /* Start with VM 0 */
    console_switching_enabled = true;
    in_escape_sequence        = false;

    logger_info("Virtual PL011 global initialized\n");
    logger_info("Console switching enabled. Use ESC+h for help\n");
}

vpl011_t *
alloc_vpl011(void)
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

vpl011_state_t *
alloc_vpl011_state(void)
{
    if (_vpl011_state_num >= VM_NUM_MAX) {
        logger_error("No more vpl011 state can be allocated!\n");
        return NULL;
    }

    vpl011_state_t *vuart = &_vpl011_states[_vpl011_state_num++];
    vpl011_state_init(vuart);
    return vuart;
}

vpl011_state_t *
get_vpl011_by_vm(struct _vm_t *vm)
{
    if (!vm || !vm->vpl011 || !vm->vpl011->state) {
        return NULL;
    }

    return vm->vpl011->state;
}

void
vpl011_state_init(vpl011_state_t *vuart)
{
    memset(vuart, 0, sizeof(vpl011_state_t));

    /* Initialize registers to reset values */
    vuart->dr    = 0x00;
    vuart->rsr   = 0x00;
    vuart->fr    = VUART_FR_TXFE | VUART_FR_RXFE; /* TX FIFO empty, RX FIFO empty */
    vuart->ilpr  = 0x00;
    vuart->ibrd  = 0x00;
    vuart->fbrd  = 0x00;
    vuart->lcr_h = 0x00;
    vuart->cr    = 0x300; /* TXE=1, RXE=1 (TX enabled, RX enabled) */
    vuart->ifls  = 0x12;  /* Default FIFO levels */
    vuart->imsc  = 0x00;  /* All interrupts masked */
    vuart->ris   = 0x00;  /* No raw interrupts */
    vuart->mis   = 0x00;  /* No masked interrupts */

    /* Virtual UART specific state */
    vuart->base_addr   = 0x09000000; /* Default PL011 base address */
    vuart->irq_num     = 33;         /* Default PL011 IRQ */
    vuart->initialized = true;

    /* Initialize FIFO state */
    vuart->tx_count = 0;
    vuart->rx_count = 0;

    logger_info("Virtual PL011 state initialized\n");
}

/* --------------------------------------------------------
 * ==================    寄存器访问    ==================
 * -------------------------------------------------------- */

uint32_t
vpl011_read(vpl011_state_t *vuart, uint32_t offset)
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

                // logger_vpl011_debug("VUART read DR: 0x%x ('%c'), rx_count=%d\n", data, (char)data, vuart->rx_count);

                /* Update interrupts after reading */
                vpl011_update_interrupts(vuart);

                return data;
            } else {
                logger_vpl011_debug("VUART read DR: RX FIFO empty, returning 0\n");
                return 0;
            }

        case VUART_RSR:
            return vuart->rsr;

        case VUART_FR:
            /* Update flag register based on FIFO state */
            vuart->fr &= ~(VUART_FR_TXFE | VUART_FR_RXFE | VUART_FR_TXFF | VUART_FR_RXFF);
            if (vuart->tx_count == 0)
                vuart->fr |= VUART_FR_TXFE;
            if (vuart->tx_count >= 16)
                vuart->fr |= VUART_FR_TXFF;
            if (vuart->rx_count == 0)
                vuart->fr |= VUART_FR_RXFE;
            if (vuart->rx_count >= 16)
                vuart->fr |= VUART_FR_RXFF;
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

        /* Peripheral ID Registers */
        case VUART_PERIPHID0:
            return VUART_PERIPHID0_VAL;
        case VUART_PERIPHID1:
            return VUART_PERIPHID1_VAL;
        case VUART_PERIPHID2:
            return VUART_PERIPHID2_VAL;
        case VUART_PERIPHID3:
            return VUART_PERIPHID3_VAL;

        /* PrimeCell ID Registers */
        case VUART_PCELLID0:
            return VUART_PCELLID0_VAL;
        case VUART_PCELLID1:
            return VUART_PCELLID1_VAL;
        case VUART_PCELLID2:
            return VUART_PCELLID2_VAL;
        case VUART_PCELLID3:
            return VUART_PCELLID3_VAL;

        default:
            logger_warn("VUART read unknown offset: 0x%x\n", offset);
            return 0;
    }
}

void
vpl011_write(vpl011_state_t *vuart, uint32_t offset, uint32_t value)
{
    if (!vuart || !vuart->initialized) {
        return;
    }

    switch (offset) {
        case VUART_DR:
            // logger_vpl011_debug("VUART write DR: 0x%x (char: '%c')\n", value, (char)(value & 0xFF));

            /* Add to TX FIFO if not full */
            if (vuart->tx_count < 16) {
                char output_char                  = (char) (value & 0xFF);
                vuart->tx_fifo[vuart->tx_count++] = (uint8_t) (value & 0xFF);

                /* Update flag register */
                vuart->fr &= ~VUART_FR_TXFE;
                if (vuart->tx_count >= 16) {
                    vuart->fr |= VUART_FR_TXFF;
                }

                /* Forward to physical UART with VM prefix if not current console */
                vpl011_output_char_with_prefix(vuart, output_char);

                /* 检查字符串匹配并可能输出时间戳 */
                // check_string_match_and_output_timestamp(output_char);

                /* Simulate immediate transmission - remove from TX FIFO */
                vuart->tx_count--;
                if (vuart->tx_count == 0) {
                    vuart->fr |= VUART_FR_TXFE;
                }
                vuart->fr &= ~VUART_FR_TXFF;

                /* Update interrupts after writing */
                vpl011_update_interrupts(vuart);
            } else {
                logger_warn("VUART: TX FIFO full, dropping char '%c'\n", (char) (value & 0xFF));
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
            logger_vpl011_debug("VUART write CR: 0x%x\n", vuart->cr);
            break;

        case VUART_IFLS:
            vuart->ifls = value & 0x3F;
            break;

        case VUART_IMSC:
            vuart->imsc = value & 0x7FF;
            logger_vpl011_debug("VUART write IMSC: 0x%x\n", vuart->imsc);
            /* Update interrupts when mask changes */
            vpl011_update_interrupts(vuart);
            break;

        case VUART_ICR:
            /* Write 1 to clear interrupt bits */
            vuart->ris &= ~(value & 0x7FF);
            logger_vpl011_debug("VUART clear interrupts: 0x%x\n", value);
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

int32_t
vpl011_mmio_handler(uint64_t gpa, uint32_t reg_num, uint32_t len, uint64_t *reg_data, bool is_write)
{
    /* Get current task and its vpl011 state */
    tcb_t *current_task = curr_task_el2();
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
        uint32_t value = (uint32_t) (*reg_data);
        vpl011_write(vuart, offset, value);
        // logger_vpl011_debug("VUART MMIO write: offset=0x%x, value=0x%x\n", offset, value);
    } else {
        /* MMIO Read */
        uint32_t value = vpl011_read(vuart, offset);
        *reg_data      = value;
        // logger_vpl011_debug("VUART MMIO read: offset=0x%x, value=0x%x\n", offset, value);
    }

    return 1; /* Successfully handled */
}

/* --------------------------------------------------------
 * ==================    中断处理    ==================
 * -------------------------------------------------------- */

void
vpl011_update_interrupts(vpl011_state_t *vuart)
{
    if (!vuart || !vuart->initialized) {
        return;
    }

    /* Update raw interrupt status based on FIFO state and conditions */
    vuart->ris = 0;

    /* RX interrupt: FIFO not empty and RX enabled */
    if (vuart->rx_count > 0 && (vuart->cr & (1 << 9))) {
        vuart->ris |= (1 << 4); /* RXIM */
    }

    /* TX interrupt: FIFO not full and TX enabled */
    if (vuart->tx_count < 16 && (vuart->cr & (1 << 8))) {
        vuart->ris |= (1 << 5); /* TXIM */
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
                    // logger_info("VUART: Injecting SPI 33 to VM %d\n", _vpl011s[i].vm->vm_id);
                    vgic_inject_spi(primary_vcpu, 33); /* PL011 IRQ is 33 */
                }
                break;
            }
        }
    }
}

void
vpl011_inject_rx_char(vpl011_state_t *vuart, char c)
{
    if (!vuart || !vuart->initialized) {
        return;
    }

    /* Add character to RX FIFO if not full */
    if (vuart->rx_count < 16) {
        vuart->rx_fifo[vuart->rx_count++] = (uint8_t) c;

        /* Update flag register - RX FIFO no longer empty */
        vuart->fr &= ~VUART_FR_RXFE;
        if (vuart->rx_count >= 16) {
            vuart->fr |= VUART_FR_RXFF;
        }

        logger_vpl011_debug("VUART: Injected char '%c' (0x%02x), rx_count=%d\n",
                            c,
                            (unsigned char) c,
                            vuart->rx_count);

        /* Update interrupts */
        vpl011_update_interrupts(vuart);
    } else {
        logger_warn("VUART: RX FIFO full, dropping char '%c'\n", c);
    }
}

void
vpl011_handle_physical_uart_rx(char c)
{
    /* Handle console switching escape sequences */
    if (console_switching_enabled) {
        if (c == 0x1B) { /* ESC key */
            in_escape_sequence = true;
            return;
        }

        if (in_escape_sequence) {
            in_escape_sequence = false;

            /* Handle escape commands */
            switch (c) {
                case '0': /* ESC+0: Switch to hypervisor console */
                    vpl011_switch_to_hypervisor();
                    return;
                case '1': /* ESC+1: Switch to VM 0 */
                case '2': /* ESC+2: Switch to VM 1 */
                case '3': /* ESC+3: Switch to VM 2 */
                case '4': /* ESC+4: Switch to VM 3 */
                    vpl011_switch_to_vm(c - '1');
                    return;
                case 'h': /* ESC+h: Show help */
                    vpl011_show_help();
                    return;
                case 's': /* ESC+s: Show status */
                    vpl011_show_status();
                    return;
                default:
                    /* Unknown escape sequence, ignore */
                    return;
            }
        }
    }

    /* Normal character processing */
    if (current_console_vm == 0xFFFFFFFF) {
        /* Hypervisor console mode - handle commands */
        vpl011_handle_hypervisor_command(c);
        return;
    }

    /* Forward to the currently active VM only */
    if (current_console_vm < _vpl011_num && _vpl011s[current_console_vm].state &&
        _vpl011s[current_console_vm].vm) {
        vpl011_inject_rx_char(_vpl011s[current_console_vm].state, c);
    }
}

/* --------------------------------------------------------
 * ==================    VM控制台切换    ==================
 * -------------------------------------------------------- */

void
vpl011_switch_to_vm(uint32_t vm_id)
{
    if (vm_id >= _vpl011_num || !_vpl011s[vm_id].vm) {
        uart_putstr("\r\n[CONSOLE] Invalid VM ID: ");
        uart_putchar('0' + vm_id);
        uart_putstr("\r\n");
        return;
    }

    current_console_vm = vm_id;
    uart_putstr("\r\n[CONSOLE] Switched to VM ");
    uart_putchar('0' + vm_id);
    uart_putstr(" (");
    if (_vpl011s[vm_id].vm->vm_name[0] != '\0') {
        uart_putstr(_vpl011s[vm_id].vm->vm_name);
    } else {
        uart_putstr("unnamed");
    }
    uart_putstr(")\r\n");
}

void
vpl011_switch_to_hypervisor(void)
{
    current_console_vm = 0xFFFFFFFF; /* Special value for hypervisor */
    uart_putstr("\r\n[CONSOLE] Switched to Hypervisor console\r\n");
    uart_putstr("Type 'help' for available commands\r\n");
}

void
vpl011_show_help(void)
{
    uart_putstr("\r\n=== Console Switching Help ===\r\n");
    uart_putstr("ESC + 0    : Switch to hypervisor console\r\n");
    uart_putstr("ESC + 1-4  : Switch to VM 0-3\r\n");
    uart_putstr("ESC + h    : Show this help\r\n");
    uart_putstr("ESC + s    : Show status\r\n");
    uart_putstr("==============================\r\n");
}

void
vpl011_show_status(void)
{
    uart_putstr("\r\n=== Console Status ===\r\n");

    if (current_console_vm == 0xFFFFFFFF) {
        uart_putstr("Current: Hypervisor console\r\n");
    } else {
        uart_putstr("Current: VM ");
        uart_putchar('0' + current_console_vm);
        uart_putstr("\r\n");
    }

    uart_putstr("Available VMs:\r\n");
    for (uint32_t i = 0; i < _vpl011_num; i++) {
        if (_vpl011s[i].vm) {
            uart_putstr("  VM ");
            uart_putchar('0' + i);
            uart_putstr(": ");
            if (_vpl011s[i].vm->vm_name[0] != '\0') {
                uart_putstr(_vpl011s[i].vm->vm_name);
            } else {
                uart_putstr("unnamed");
            }
            if (i == current_console_vm) {
                uart_putstr(" (active)");
            }
            uart_putstr("\r\n");
        }
    }
    uart_putstr("======================\r\n");
}

uint32_t
vpl011_get_current_console_vm(void)
{
    return current_console_vm;
}

void
vpl011_set_console_switching(bool enabled)
{
    console_switching_enabled = enabled;
    uart_putstr("\r\n[CONSOLE] Console switching ");
    uart_putstr(enabled ? "enabled" : "disabled");
    uart_putstr("\r\n");
}

/* --------------------------------------------------------
 * ==================    输出处理    ==================
 * -------------------------------------------------------- */

static uint32_t
find_vm_id_by_vuart(vpl011_state_t *vuart)
{
    for (uint32_t i = 0; i < _vpl011_num; i++) {
        if (_vpl011s[i].state == vuart) {
            return i;
        }
    }
    return 0xFFFFFFFF; /* Not found */
}

static void
vpl011_output_char_with_prefix(vpl011_state_t *vuart, char c)
{
    static bool at_line_start = true;
    uint32_t    vm_id         = find_vm_id_by_vuart(vuart);

    /* If this is the current console VM, output directly without prefix */
    if (vm_id == current_console_vm) {
        uart_putchar(c);
        at_line_start = (c == '\n');
        return;
    }

    /* For non-current VMs, add prefix at line start */
    if (at_line_start && c != '\n' && c != '\r') {
        uart_putstr("[VM");
        uart_putchar('0' + vm_id);
        uart_putstr("] ");
        at_line_start = false;
    }

    uart_putchar(c);

    if (c == '\n') {
        at_line_start = true;
    }
}

/* --------------------------------------------------------
 * ==================    Hypervisor命令处理    ==================
 * -------------------------------------------------------- */

#define HV_CMD_BUFFER_SIZE 64
static char     hv_cmd_buffer[HV_CMD_BUFFER_SIZE];
static uint32_t hv_cmd_pos = 0;

static void
vpl011_handle_hypervisor_command(char c)
{
    /* Echo the character */
    uart_putchar(c);

    if (c == '\r' || c == '\n') {
        /* End of command */
        uart_putstr("\r\n");
        hv_cmd_buffer[hv_cmd_pos] = '\0';

        if (hv_cmd_pos > 0) {
            vpl011_execute_hypervisor_command(hv_cmd_buffer);
        }

        /* Reset buffer and show prompt */
        hv_cmd_pos = 0;
        uart_putstr("hypervisor> ");
    } else if (c == '\b' || c == 0x7F) {
        /* Backspace */
        if (hv_cmd_pos > 0) {
            hv_cmd_pos--;
            uart_putstr("\b \b"); /* Erase character on screen */
        }
    } else if (c >= 32 && c < 127) {
        /* Printable character */
        if (hv_cmd_pos < HV_CMD_BUFFER_SIZE - 1) {
            hv_cmd_buffer[hv_cmd_pos++] = c;
        }
    }
}

static void
vpl011_execute_hypervisor_command(const char *cmd)
{
    if (strcmp(cmd, "help") == 0) {
        uart_putstr("Available commands:\r\n");
        uart_putstr("  help     - Show this help\r\n");
        uart_putstr("  status   - Show VM status\r\n");
        uart_putstr("  vm <id>  - Switch to VM console\r\n");
        uart_putstr("  list     - List all VMs\r\n");
        uart_putstr("  exit     - Exit hypervisor console\r\n");
    } else if (strcmp(cmd, "status") == 0) {
        vpl011_show_status();
    } else if (strcmp(cmd, "list") == 0) {
        uart_putstr("VM List:\r\n");
        for (uint32_t i = 0; i < _vpl011_num; i++) {
            if (_vpl011s[i].vm) {
                uart_putstr("  VM ");
                uart_putchar('0' + i);
                uart_putstr(": ");
                if (_vpl011s[i].vm->vm_name[0] != '\0') {
                    uart_putstr(_vpl011s[i].vm->vm_name);
                } else {
                    uart_putstr("unnamed");
                }
                uart_putstr("\r\n");
            }
        }
    } else if (strncmp(cmd, "vm ", 3) == 0 && cmd[3] >= '0' && cmd[3] <= '9') {
        uint32_t vm_id = cmd[3] - '0';
        vpl011_switch_to_vm(vm_id);
    } else if (strcmp(cmd, "exit") == 0) {
        if (_vpl011_num > 0) {
            vpl011_switch_to_vm(0); /* Switch to VM 0 by default */
        } else {
            uart_putstr("No VMs available to switch to\r\n");
        }
    } else {
        uart_putstr("Unknown command: ");
        uart_putstr(cmd);
        uart_putstr("\r\nType 'help' for available commands\r\n");
    }
}
