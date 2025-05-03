#include "sys/tcr.h"
#include "sys/sys.h"
#include "mem/mmu.h"

.global init_mmu
.global init_mmu_el2

.macro enable_mmu sctlr tmp
	mrs     \tmp, \sctlr
    /* Enable MMU */
	orr     \tmp, \tmp, #SCTLR_EL1_M
    /* Disable alignment checking */
	bic     \tmp, \tmp, #SCTLR_EL1_A
	bic     \tmp, \tmp, #SCTLR_EL1_SA0
	bic     \tmp, \tmp, #SCTLR_EL1_SA
	orr     \tmp, \tmp, #SCTLR_EL1_nAA
	/* Data accesses Cacheable */
    orr     \tmp, \tmp, #SCTLR_EL1_C
    /* Instruction access Cacheable */
	orr     \tmp, \tmp, #SCTLR_EL1_I
	msr     \sctlr, \tmp
	isb
.endm

.macro disable_mmu sctlr tmp
	mrs     \tmp, \sctlr
    /* Disable MMU */
	bic     \tmp, \tmp, #SCTLR_EL1_M
    /* Disable alignment checking */
	bic     \tmp, \tmp, #SCTLR_EL1_A
	bic     \tmp, \tmp, #SCTLR_EL1_SA0
	bic     \tmp, \tmp, #SCTLR_EL1_SA
	orr     \tmp, \tmp, #SCTLR_EL1_nAA
    /* Disable Data Cache */
	bic     \tmp, \tmp, #SCTLR_EL1_C
    /* Disable Instruction Cache */
	bic     \tmp, \tmp, #SCTLR_EL1_I
	msr     \sctlr, \tmp
	isb
.endm

/* ldr x0, =0x40084000 need change */
init_mmu:
    // 设置 Translation Table Base Register 0 (TTBR0_EL1)
    // ldr x0, =0x40088000
    msr ttbr0_el1, x0
    msr ttbr1_el1, x1
    
    // 设置 Translation Control Register (TCR_EL1)
    ldr x0, =(TCR_EL1)
    msr tcr_el1, x0
    // 设置 MAIR_EL1 (Memory Attribute Indirection Register)
    ldr x0, =(MAIR_VALUE)
    msr mair_el1, x0
    

    //enable_mmu sctlr_el1 x1
    // 设置 SCTLR_EL1 (System Control Register) 以启用MMU
    mrs x0, sctlr_el1
    orr x0, x0, #(1 << 0) // 设置 SCTLR_EL1.M 位以启用 MMU
    msr sctlr_el1, x0
    
    // 清空TLB
    dsb sy // 确保之前所有内存访问完成
    isb    // 确保所有指令都执行完成

    ret

/* ldr x0, =0x40084000 need change */
init_mmu_el2:
    // 设置 Translation Table Base Register 0 (TTBR0_EL2)
    // ldr x0, =0x40088000
    msr ttbr0_el2, x0
    // msr ttbr1_el2, x0
    
    // 设置 Translation Control Register (TCR_EL2)
    ldr x0, =(TCR_EL2)
    msr tcr_el2, x0
    // 设置 MAIR_EL2 (Memory Attribute Indirection Register)
    ldr x0, =(MAIR_VALUE)
    msr mair_el2, x0
    

    enable_mmu sctlr_el2 x0
    
    // 清空TLB
    dsb sy // 确保之前所有内存访问完成
    isb    // 确保所有指令都执行完成

    ret