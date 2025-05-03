
#include "sys/sys.h"

.global guest_start
.global test_guest

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
	/*
	hvc #0
	ldr x0, =0x50000000
	ldr x6, [x0]
	ldr x0, =0x50000004
	ldr x6, [x0]
	ldr x0, =0x50000040
	ldr x6, [x0]
	ldr x0, =0x50000400
	ldr x6, [x0]
	ldr x0, =0x50000fff
	ldr x6, [x0]
	
	ldr x0, =0x50010000
	ldr x6, [x0]
	*/
	b .  // el1


.section .text 
guest_start:

	adrp    x1,:got:test_guest
	ldr     x1,[x1,:got_lo12:test_guest]
	msr elr_el2, x1

	// 只在这个地方设置 spsr_el2 
	msr	spsr_el2, xzr
	ldr x0, =SPSR_VALUE  // EL1h | SPSR_FIQ_MASK | SPSR_IRQ_MASK | SPSR_ABT_MASK
	msr spsr_el2, x0
	isb  

	ldr x0, =0x30C50830  // arceos
	msr sctlr_el1, x0
	isb  

	mov x0, #0x0
	eret


.section .data.guset_bin
	// .incbin "../guest/nimbos/nimbos.bin"
	// .incbin "../guest/testos/build/kernel.bin"
	.incbin "../guest/linux/linux.bin"

.section .data.guest_dtb
  	.incbin "../guest/linux/linux.dtb"

.section .data.guest_fs
  	.incbin "../guest/linux/initrd.gz"