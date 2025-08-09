
#include "sys/sys.h"

.global guest_start
.global test_guest

.align 12
test_guest:
	mov x0, 100
	mrs x0, CurrentEL          // Read current exception level
	lsr x0, x0, #2             // 右移两位

	mov x0, #0
	mov x1, #1
	mov x2, #2
	mov x3, #3
	mov x4, #4
	mov x5, #5
	mov x6, #0x17
	mov x7, #7
	mov x8, #8
	mov x9, #9
	mov x10, #10

	/* 目前这条指令会陷入 el2 异常 */
	// mrs x0, CurrentEL
	msr sctlr_el1, x0
	mrs	x3, ctr_el0
	mov x10, #10

	// 循环串口输出测试
	mov x1, #0x9000000         // UART基地址

loop:
	// 输出 "Hi\n"
	mov w0, #'H'
	str w0, [x1]
	mov w0, #'i'
	str w0, [x1]
	mov w0, #'\n'
	str w0, [x1]

	// 延时 - 很多nop指令
	mov x2, #0x900000          // 延时计数器
delay:
	nop
	nop
	nop
	nop
	nop
	subs x2, x2, #1
	b.ne delay

	b loop                     // 继续循环