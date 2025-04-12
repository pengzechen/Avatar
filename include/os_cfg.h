

#ifndef __OS_CFG_H__
#define __OS_CFG_H__



/* 设备定义区 */
// UART
#define UART0_BASE     0x09000000UL   // PL011 UART 基地址
// GIC
#define GICD_BASE_ADDR 0x8000000UL    // unsigned long 64 位
#define GICC_BASE_ADDR 0x8010000UL
#define GICH_BASE_ADDR 0x8030000UL
#define GICV_BASE_ADDR 0x8040000UL


/* 内存定义区 */

#define KERNEL_RAM_START   (0UL)               // 内存从0地址开始
#define KERNEL_RAM_END     (0x40000000UL * 4)  // 4 GB

#define PAGE_SIZE  (4096)
// bit个数 = 内存大小 / 页大小
#define OS_CFG_BITMAP_SIZE  (KERNEL_RAM_END - KERNEL_RAM_START) / PAGE_SIZE


/* 内核线程栈定义区 */
#define STACK_SIZE  (1 << 14)   // 16 K

/* 核数 */
#ifndef SMP_NUM
#define SMP_NUM   1
#endif


/* guest内存定义区 */
#define GUEST_RAM_START   ((unsigned long long)0x40000000)
#define GUEST_RAM_END     (GUEST_RAM_START + 0x40000000)

#define GUEST_KERNEL_START  0x70200000UL
#define GUEST_DTB_START     0x70000000UL
#define GUEST_FS_START      0x78000000UL

#define MMIO_AREA_GICD  0x8000000UL
#define MMIO_AREA_GICC  0x8010000UL

#define KERNEL_VMA 0xffff000000000000

/* 任务定义区 */
#define MAX_TASKS       64

#define SYS_TASK_TICK   10

// 测试
#define MMIO_ARREA      0x50000000

#endif // __OS_CFG_H__