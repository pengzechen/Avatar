

.global switch_context

// 参数 x0: 当前任务的 TCB 指针
// 参数 x1: 下一个任务的 TCB 指针
switch_context:
    # 首先保存当前的callee-saved register
    # 第一个参数在x0
    stp     x19, x20, [x0, #16*0]
    stp     x21, x22, [x0, #16*1]
    stp     x23, x24, [x0, #16*2]
    stp     x25, x26, [x0, #16*3]
    stp     x27, x28, [x0, #16*4]
    stp     x29, x30, [x0, #16*5]
    mrs     x19, tpidr_el0
    mov     x20, sp
    stp     x19, x20, [x0, #16*6]

    # 获得给定的struct context* new
    # 因为它是第二个参数 所以 *new 在x1
    # 装载新的context 到寄存器上
    ldp     x23, x24, [x1, #16*6]
    msr     tpidr_el0, x23
    mov     sp, x24
    ldp     x19, x20, [x1, #16*0]
    ldp     x21, x22, [x1, #16*1]
    ldp     x23, x24, [x1, #16*2]
    ldp     x25, x26, [x1, #16*3]
    ldp     x27, x28, [x1, #16*4]
    ldp     x29, x30, [x1, #16*5]

    msr daifclr, #2
    ret                    // 返回，执行下一个任务

/*
stp: Store Pair of registers. 将两个寄存器的值存储到内存中。
ldp: Load Pair of registers. 从内存中加载两个寄存器的值。
str: Store Register. 将一个寄存器的值存储到内存中。
ldr: Load Register. 从内存中加载一个寄存器的值。
mov: Move Register. 将一个寄存器的值移动到另一个寄存器。
 */

 .global get_el
get_el:
	mrs x0, CurrentEL
	lsr x0, x0, #2
	ret

