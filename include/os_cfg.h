

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

// ram fs 起始地址
#define RAM_FS_MEM_START 0x50000000

/* 内核线程栈定义区 */
#define STACK_SIZE (1 << 14) // 16 K

/* 核数 */
#ifndef SMP_NUM
#define SMP_NUM 1
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

extern char __kernal_start[];
extern char __heap_flag[];

/* 任务定义区 */
#define MAX_TASKS 64

#define SYS_TASK_TICK 10

// 测试
#define MMIO_ARREA 0x50000000

#endif // __OS_CFG_H__