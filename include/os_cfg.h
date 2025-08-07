

#ifndef __OS_CFG_H__
#define __OS_CFG_H__

/* 设备定义区 */
// UART
#define UART0_BASE 0x09000000UL // PL011 UART 基地址
// GIC
#define GICD_BASE_ADDR 0x8000000UL // unsigned long 64 位
#define GICC_BASE_ADDR 0x8010000UL
#define GICH_BASE_ADDR 0x8030000UL
#define GICV_BASE_ADDR 0x8040000UL

/* 内存定义区 */

#define KERNEL_RAM_START (0x40000000UL)
#define KERNEL_RAM_SIZE (0xc0000000UL) // 4GB
#define KERNEL_RAM_END (KERNEL_RAM_START + KERNEL_RAM_SIZE)

#define PAGE_SIZE (4096)
// bit个数 = 内存大小 / 页大小
#define OS_CFG_BITMAP_SIZE (KERNEL_RAM_END - KERNEL_RAM_START) / PAGE_SIZE

/* 核数 - 必须在最前面定义，因为后面的配置会用到 */
#ifndef SMP_NUM
#define SMP_NUM 1
#endif

// ram fs 起始地址
#define RAM_FS_MEM_START 0x50000000

/* 内核线程栈定义区 */
#define STACK_SIZE (1 << 14) // 16 K
#define BOOT_STACK_SIZE STACK_SIZE  // 启动栈大小
#define IDLE_STACK_SIZE (1 << 12)   // 4K idle栈

/* SMP相关栈配置 - 基于SMP_NUM动态计算 */
#define SECONDARY_CPU_COUNT (SMP_NUM - 1)  // 从核数量

/* 汇编用栈配置 - 根据SMP_NUM计算从核栈总大小 */
#if SMP_NUM <= 1
#define SECONDARY_STACK_TOTAL BOOT_STACK_SIZE  // 至少分配一个栈空间
#elif SMP_NUM == 2
#define SECONDARY_STACK_TOTAL BOOT_STACK_SIZE
#elif SMP_NUM == 3
#define SECONDARY_STACK_TOTAL (BOOT_STACK_SIZE * 2)
#elif SMP_NUM == 4
#define SECONDARY_STACK_TOTAL (BOOT_STACK_SIZE * 3)
#elif SMP_NUM == 5
#define SECONDARY_STACK_TOTAL (BOOT_STACK_SIZE * 4)
#elif SMP_NUM == 6
#define SECONDARY_STACK_TOTAL (BOOT_STACK_SIZE * 5)
#elif SMP_NUM == 7
#define SECONDARY_STACK_TOTAL (BOOT_STACK_SIZE * 6)
#elif SMP_NUM == 8
#define SECONDARY_STACK_TOTAL (BOOT_STACK_SIZE * 7)
#else
#define SECONDARY_STACK_TOTAL (BOOT_STACK_SIZE * 7)  // 最大支持8核
#endif

/* HV */
#ifndef HV
#define HV 0
#endif

/* guest内存定义区 */
#define GUEST_RAM_START ((unsigned long long)0x40000000)
#define GUEST_RAM_END (GUEST_RAM_START + 0x40000000)

#define MMIO_AREA_GICD 0x8000000UL
#define MMIO_AREA_GICC 0x8010000UL

#if HV == 1
// 这里是因为vmm需要申请内存
#define KERNEL_VMA 0ULL
#else
#define KERNEL_VMA 0xffff000000000000ULL
#endif

#ifndef __ASSEMBLER__
extern char __kernal_start[];
extern char __heap_flag[];
#endif

/* 任务定义区 */
#define MAX_TASKS 64

#define SYS_TASK_TICK 10

// 测试
#define MMIO_ARREA 0x50000000

#endif // __OS_CFG_H__