
#ifndef __OS_CFG_H__
#define __OS_CFG_H__

/* ============================================================================
 * 系统配置参数 - 统一管理所有硬编码值
 * ============================================================================ */

/* HV 模式配置 - 必须在最前面定义，因为后面的配置会用到 */
#ifndef HV
    #define HV 1
#endif

/* 核数配置 - 必须在最前面定义，因为后面的配置会用到 */
#ifndef SMP_NUM
    #define SMP_NUM 1
#endif

/* 设备基地址配置 */
// UART
#define UART0_BASE_ADDR 0x09000000UL  // PL011 UART 基地址
// GIC
#define GICD_BASE_ADDR 0x8000000UL  // unsigned long 64 位
#define GICC_BASE_ADDR 0x8010000UL
#define GICH_BASE_ADDR 0x8030000UL
#define GICV_BASE_ADDR 0x8040000UL

/* 内存配置 */
#define KERNEL_RAM_START (0x40000000UL)
#define KERNEL_RAM_SIZE  (0xc0000000UL)  // 4GB
#define KERNEL_RAM_END   (KERNEL_RAM_START + KERNEL_RAM_SIZE)

#define PAGE_SIZE (4096)
// bit个数 = 内存大小 / 页大小
#define OS_CFG_BITMAP_SIZE (KERNEL_RAM_END - KERNEL_RAM_START) / PAGE_SIZE

// ram fs 起始地址
#define RAM_FS_MEM_START 0x50000000

/* 定时器配置 */
#define TIMER_FREQUENCY_HZ     62500000  // 定时器频率 62.5MHz
#define TIMER_TICK_INTERVAL_MS 10        // 定时器中断间隔 10ms
#define TIMER_TVAL_VALUE       (TIMER_FREQUENCY_HZ * TIMER_TICK_INTERVAL_MS / 1000)  // 625000

/* 中断向量配置 */
#if HV
    #define TIMER_VECTOR 26  // Hypervisor Timer (PPI 10) - 当HV=1时使用
#else
    #define TIMER_VECTOR 30  // Non-secure Physical Timer (PPI 14) - 当HV=0时使用
#endif

#define VIRTUAL_TIMER_IRQ 27  // Virtual Timer (PPI 11) - 用于虚拟定时器中断
#define PL011_INT         33  // UART 中断

/* 内核线程栈定义区 */
#define STACK_SIZE      (1 << 14)   // 16 K
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


/* guest内存定义区 */
#define GUEST_RAM_START ((unsigned long long) 0x40000000)
#define GUEST_RAM_END   (GUEST_RAM_START + 0x40000000)

#define MMIO_AREA_GICD  0x8000000UL
#define MMIO_AREA_GICC  0x8010000UL
#define MMIO_AREA_PL011 0x09000000UL

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

/* 任务配置 */
#define MAX_TASKS             64
#define TASK_PRIORITY_LEVELS  8
#define TASK_PRIORITY_DEFAULT 4
#define SYS_TASK_TICK         10

/* 调度配置 */
#define SCHEDULE_TIME_SLICE_MS     100
#define SCHEDULE_PREEMPT_THRESHOLD 5

/* 内存池配置 */
#define MEMORY_POOL_SIZE      (64 * 1024 * 1024)  // 64MB
#define MEMORY_POOL_ALIGNMENT 8
#define HEAP_OFFSET           0x900000  // 9MB offset for heap

/* 虚拟化配置 */
#define VM_NUM_MAX   4
#define VCPU_NUM_MAX 8

/* Guest内存配置 */
#define GUEST_RAM_SIZE 0x40000000  // 1GB

/* 测试和调试配置 */
#define TEST_MEM_ADDR 0x9000000
#define TEST_MEM_MASK 97
#define MMIO_ARREA    0x50000000  // 测试用MMIO区域

/* 等待循环配置 */
#define CPU_STARTUP_WAIT_LOOPS  10
#define CPU_STARTUP_INNER_LOOPS 0xfffff

/* 页表配置 */
#define PAGE_TABLE_MAX_ENTRIES_L0 1
#define PAGE_TABLE_MAX_ENTRIES_L1 4
#define PAGE_TABLE_MAX_ENTRIES_L2 512
#define PAGE_TABLE_MAX_ENTRIES_L3 512

/* 内存映射配置 */
#define DEVICE_MEM_START 0x8000000
#define DEVICE_MEM_END   0xa000000

#endif  // __OS_CFG_H__