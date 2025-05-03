

#ifndef __PL011_H__
#define __PL011_H__

#include "aj_types.h"
#include "os_cfg.h"

#define UART0_DR (*(volatile uint32_t *)(UART0_BASE + 0x00))   // 数据寄存器
#define UART0_FR (*(volatile uint32_t *)(UART0_BASE + 0x18))   // 标志寄存器
#define UART0_IBRD (*(volatile uint32_t *)(UART0_BASE + 0x24)) // 整数波特率除数寄存器
#define UART0_FBRD (*(volatile uint32_t *)(UART0_BASE + 0x28)) // 小数波特率除数寄存器
#define UART0_LCRH (*(volatile uint32_t *)(UART0_BASE + 0x2C)) // 线路控制寄存器
#define UART0_CR (*(volatile uint32_t *)(UART0_BASE + 0x30))   // 控制寄存器
#define UART0_IFLS (*(volatile uint32_t *)(UART0_BASE + 0x34)) // 中断队列的等级选择
#define UART0_IMSC (*(volatile uint32_t *)(UART0_BASE + 0x38)) // 中断掩码集/清除寄存器
#define UART0_RIS (*(volatile uint32_t *)(UART0_BASE + 0x3c)) // raw interrupt status reg
#define UART0_MIS (*(volatile uint32_t *)(UART0_BASE + 0x40)) // mask interrupt status reg
#define UART0_ICR (*(volatile uint32_t *)(UART0_BASE + 0x44))   // interrupt clear reg
#define UART0_DMACR (*(volatile uint32_t *)(UART0_BASE + 0x48)) // dmacr

#define UART0_PERIPHID_0  (*(volatile uint32_t *)(UART0_BASE + 0x0fe0))  // periphid 0
#define UART0_PERIPHID_1  (*(volatile uint32_t *)(UART0_BASE + 0x0fe4))  // periphid 1
#define UART0_PERIPHID_2  (*(volatile uint32_t *)(UART0_BASE + 0x0fe8))  // periphid 2
#define UART0_PERIPHID_3  (*(volatile uint32_t *)(UART0_BASE + 0x0fec))  // periphid 3
#define UART0_PCELLID_0  (*(volatile uint32_t *)(UART0_BASE + 0x0ff0))   // pcellid 0
#define UART0_PCELLID_1  (*(volatile uint32_t *)(UART0_BASE + 0x0ff4))   // pcellid 1
#define UART0_PCELLID_2  (*(volatile uint32_t *)(UART0_BASE + 0x0ff8))   // pcellid 2
#define UART0_PCELLID_3  (*(volatile uint32_t *)(UART0_BASE + 0x0ffc))   // pcellid 3

extern void uart_init();
extern char uart_getc();
extern void uart_putc(char);

#endif // __PL011_H__