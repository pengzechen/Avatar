
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

	b .  // el1


.section .data.guset_bin
	// .incbin "../guest/nimbos/nimbos.bin"
	// .incbin "../guest/testos/build/kernel.bin"
	.incbin "../guest/linux/linux.bin"

.section .data.guest_dtb
  	.incbin "../guest/linux/linux.dtb"

.section .data.guest_fs
  	.incbin "../guest/linux/initrd.gz"