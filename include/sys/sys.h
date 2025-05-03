// https://developer.arm.com/documentation/ddi0595/2021-03/AArch64-Registers/

#ifndef __SYS_H__
#define __SYS_H__


#define BIT(x) (1 << (x))


/*   ==============  HCR_EL2 ================ */
// https://developer.arm.com/documentation/ddi0595/2020-12/AArch64-Registers/HCR-EL2--Hypervisor-Configuration-Register?lang=en


// trap related
#define HCR_TACR (1 << 21)
#define HCR_TSC  (1 << 19)
#define HCR_TID3 (1 << 18)
#define HCR_TID2 (1 << 17)
#define HCR_TID1 (1 << 16)
#define HCR_TWE	 (1 << 14)
#define HCR_TWI	 (1 << 13)
// others
#define HCR_E2H	 (0 << 34)
#define HCR_RW	 (1 << 31)
#define HCR_TGE	 (0 << 27)
#define HCR_AMO	 (1 << 5) // routing to EL2
#define HCR_IMO	 (1 << 4) // routing to EL2
#define HCR_FMO	 (1 << 3) // routing to EL2
#define HCR_SWIO (1 << 1)
#define HCR_VM	 (1 << 0) // stage 2 translation enable



#define HCR_VALUE_NO_ROUTE                                                 \
	(HCR_TACR | HCR_TID3 | HCR_TID1 | HCR_TWE | HCR_TWI |       \
	 HCR_E2H | HCR_RW | HCR_TGE | HCR_SWIO | HCR_VM)
#define HCR_VALUE_ROUTE                                                    \
	(HCR_TACR | HCR_TSC | HCR_TWE |    \
	 HCR_E2H | HCR_RW | HCR_TGE | HCR_AMO | HCR_IMO | HCR_FMO | HCR_SWIO | HCR_VM)
#define HCR_VALUE HCR_VALUE_ROUTE
// #define HCR_VALUE HCR_VALUE_NO_ROUTE




/* =====================   SPSR_EL2 ======================= */
/*
0b0000	EL0t.
0b0100	EL1t.
0b0101	EL1h.
0b1000	EL2t.
0b1001	EL2h.
*/

#define SPSR_MASK_ALL    (7 << 6)
#define SPSR_FIQ_MASK    (1 << 6)        /* Fast Interrupt mask */
#define SPSR_IRQ_MASK    (1 << 7)        /* Interrupt mask */
#define SPSR_ABT_MASK    (1 << 8)        /* Asynchronous Abort mask */
#define PSM_USR (0x10)
#define PSM_FIQ (0x11)
#define PSM_IRQ (0x12)
#define PSM_SVC (0x13)
#define PSM_MON (0x16)
#define PSM_ABT (0x17)
#define PSM_HYP (0x1a)
#define PSM_UND (0x1b)
#define PSM_SYS (0x1f)



#define SPSR_INIT SPSR_MASK_ALL    // 关闭所有中断
#define SPSR_VALUE (SPSR_MASK_ALL | 0b0101 )
#define SPSR_EL2_VALUE_IRQ (SPSR_FIQ_MASK | SPSR_ABT_MASK | 0b0101 )




/* =====================   SPSR_EL1 ======================= */

/* SPSR_EL1 寄存器 aarch64（来自 ARM-ARM C5-395 by LN） */
/* 当异常进入 EL1 时保存进程状态 */
#define SPSR_EL1_N      BIT(31) /* N 条件标志（负数标志） */
#define SPSR_EL1_Z      BIT(30) /* Z 条件标志（零标志） */
#define SPSR_EL1_C      BIT(29) /* C 条件标志（进位标志） */
#define SPSR_EL1_V      BIT(28) /* V 条件标志（溢出标志） */
#define SPSR_EL1_DIT    BIT(24) /* 数据独立定时（PSTATE.DIT，ARMv8.4） */
#define SPSR_EL1_UAO    BIT(23) /* 用户访问覆盖（PSTATE.UAO，ARMv8.2） */
#define SPSR_EL1_PAN    BIT(22) /* 特权访问永远禁止（PSTATE.PAN，ARMv8.1） */
#define SPSR_EL1_SS     BIT(21) /* 软件单步（PSTATE.SS） */
#define SPSR_EL1_IL     BIT(20) /* 非法执行状态（PSTATE.IL） */
#define SPSR_EL1_DEBUG  BIT(9)  /* 调试屏蔽（Debug mask） */
#define SPSR_EL1_SERROR BIT(8)  /* SERROR 屏蔽（SERROR mask） */
#define SPSR_EL1_IRQ    BIT(7)  /* IRQ 屏蔽（IRQ mask） */
#define SPSR_EL1_FIQ    BIT(6)  /* FIQ 屏蔽（FIQ mask） */
#define SPSR_EL1_M      BIT(4)  /* 从 AArch64 发生异常（Exception taken from AArch64） */

#define SPSR_EL1_EL0t   0b0000
#define SPSR_EL1_EL1t   0b0100
#define SPSR_EL1_EL1h   0b0101

/* SPSR_EL1 DEFAULT */
#define SPSR_EL1_KERNEL SPSR_EL1_EL1h
#define SPSR_EL1_USER   SPSR_EL1_EL0t







/* =====================   SCTLR_EL2 ======================= */

#define SCTLR_EE	       (0 << 25)
#define SCTLR_I_CACHE_DISABLED (0 << 12)
#define SCTLR_D_CACHE_DISABLED (0 << 2)
#define SCTLR_MMU_DISABLED     (0 << 0)
#define SCTLR_MMU_ENABLED      (1 << 0)

#define SCTLR_VALUE_MMU_DISABLED                                      \
	(SCTLR_EE | SCTLR_I_CACHE_DISABLED | SCTLR_D_CACHE_DISABLED | \
	 SCTLR_MMU_DISABLED)




/* =====================   SCTLR_EL1 ======================= */
/* SCTLR_EL1 System Control Register aarch64 (FROM ARM-ARM D12-3081 by LN) */

#define SCTLR_EL1_EnIA BIT(31) /* 控制指针认证的启用 */
#define SCTLR_EL1_EnIB BIT(30) /* 控制指针认证的启用 */
#define SCTLR_EL1_LSMAOE BIT(29) /* 启用 Load Multiple 和 Store Multiple 操作的原子性和排序性 */
#define SCTLR_EL1_nTLSMD BIT(28) /* 不陷入对 Device-nGRE/Device-nGnRE/Device-nGnRnE 内存的 Load Multiple 和 Store Multiple 操作 */
#define SCTLR_EL1_EnDA BIT(27) /* 控制指针认证的启用 */
#define SCTLR_EL1_UCI BIT(26) /* 在 AArch64 状态下，将 EL0 执行的缓存维护指令陷入到 EL1 */
#define SCTLR_EL1_EE BIT(25) /* 控制 EL1 数据访问和 EL1/EL0 翻译机制下一级地址转换表遍历的字节序 */
#define SCTLR_EL1_E0E BIT(24) /* 控制 EL0 数据访问的字节序 */
#define SCTLR_EL1_SPAN BIT(23) /* 在进入 EL1 异常时，设置禁止特权访问 */
#define SCTLR_EL1_IESB BIT(21) /* 启用隐式错误同步事件 */
#define SCTLR_EL1_WXN  BIT(19) /* 写权限隐含禁止执行（XN，即 Execute-never） */
#define SCTLR_EL1_nTWE BIT(18) /* 从所有执行状态中，将 EL0 执行的 WFE 指令陷入到 EL1 */
#define SCTLR_EL1_nTWI BIT(16) /* 从所有执行状态中，将 EL0 执行的 WFI 指令陷入到 EL1 */
#define SCTLR_EL1_UCT BIT(15) /* 仅在 AArch64 状态下，将 EL0 对 CTR_EL0 的访问陷入到 EL1 */
#define SCTLR_EL1_DZE BIT(15) /* 仅在 AArch64 状态下，将 EL0 执行的 DC ZVA 指令陷入到 EL1 */
#define SCTLR_EL1_EnDB BIT(13) /* 控制指针认证的启用 */
#define SCTLR_EL1_I BIT(12) /* 控制 EL0 和 EL1 下指令访问的缓存属性 */
#define SCTLR_EL1_UMA BIT(9) /* 用户屏蔽访问：将 EL0 执行访问 PSTATE.{D, A, I, F} 的 MSR/MRS 指令陷入到 EL1 */
#define SCTLR_EL1_SED BIT(8) /* 禁用 SETEND 指令：在 AArch32 的 EL0 下禁用 SETEND 指令 */
#define SCTLR_EL1_ITD BIT(7) /* 禁用 IT 指令：在 AArch32 的 EL0 下禁用某些 IT 指令的使用 */
#define SCTLR_EL1_nAA BIT(6) /* 非对齐访问：在特定条件下控制 EL1 和 EL0 产生对齐错误 */
#define SCTLR_EL1_CP15BEN BIT(5) /* 启用系统指令内存屏障（适用于 AArch32） */
#define SCTLR_EL1_SA0 BIT(4) /* 启用 EL0 的栈指针对齐检查 */
#define SCTLR_EL1_SA  BIT(3) /* 启用栈指针对齐检查 */
#define SCTLR_EL1_C   BIT(2) /* 控制数据访问的缓存属性 */
#define SCTLR_EL1_A   BIT(1) /* 启用对齐检查 */
#define SCTLR_EL1_M   BIT(0) /* 启用 EL1 和 EL0 一级地址转换的 MMU */



#endif // __SYS_H__