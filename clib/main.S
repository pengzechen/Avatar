
.global _start
_start:
    ldr x0, =stack_end
    mov sp, x0

    MRS     x0, CPACR_EL1            // 读取 CPACR_EL1 寄存器
    ORR     x0, x0, #(0xF << 20)     // 设置 CP10 和 CP11 位为 1111，启用浮点和SIMD
    MSR     CPACR_EL1, x0            // 将修改后的值写回 CPACR_EL1
    ISB

        // 禁用对齐检查
    mrs x0, sctlr_el1      // 读取 SCTLR_EL1 寄存器到 x0
    bic x0, x0, #0x1000    // 清除 A 位 (对齐检查位)
    msr sctlr_el1, x0      // 写回到 SCTLR_EL1 寄存器
    isb                    // 确保变更生效
    
    
    bl main
    // 如果 main 返回，就进入死循环
    b .