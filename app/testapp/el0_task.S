.global _start

.section .text

_start:

    bl stack_test
    b .
    bl loop

loop:
    mov x8, #0
    mov x0, #'A'
    mov x1, #'B'
    svc #0
    b loop  // 无限循环执行

stack_test:
    mov x0, #1
    mov x1, #2
    sub sp, sp, 2 * 8
    str	x0, [sp, #0]
    str	x1, [sp, #8]
    add sp, sp, 2 * 8

    ret
