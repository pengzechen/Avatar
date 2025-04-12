
#ifndef __MMU_H__
#define __MMU_H__



//  ===================== MAIR 寄存器 ===========================
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/* Memory Attributes 控制这个页表项对应的内存区域的内存类型,缓存策略 */

// 设备内存 禁止聚集(non Gathering) 禁止重排(non re-order) 禁止提前的写入ACK(Early Write Acknowledgement)
#define MA_DEVICE_nGnRnE_Flags  0x00
// 普通内存 使用写回写分配 读分配 (最快的形式,各种缓存buff拉满)
#define MA_MEMORY_Flags         0xFF         
// 普通内存 禁止所有缓存策略
#define MA_MEMORY_NoCache_Flags 0x44 

// MAIR_ELx 可以放置8种Memory Attributes,但是我们只需要这三种就够了
#define MA_DEVICE_nGnRnE   0
#define MA_MEMORY          1
#define MA_MEMORY_NoCache  2

#define PTE_AIDX_DEVICE_nGnRn   (MA_DEVICE_nGnRnE  << 2)
#define PTE_AIDX_MEMORY         (MA_MEMORY         << 2)
#define PTE_AIDX_MEMORY_NOCACHE (MA_MEMORY_NoCache << 2)


// 这个值 我们一会放到MAIR_EL1 寄存器中
#define MAIR_VALUE                                                \
        (MA_DEVICE_nGnRnE_Flags  << (8 * MA_DEVICE_nGnRnE)) |     \
        (MA_MEMORY_Flags         << (8 * MA_MEMORY)) |            \
        (MA_MEMORY_NoCache_Flags << (8 * MA_MEMORY_NoCache))



//  ====================== 页表项的配置 ============================
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Attribute fields in stage 1 VMSAv8-64 Block and Page descriptors:

// Whether the descriptor is valid.
#define VALID (1ULL << 0)

// The descriptor gives the address of the next level of translation table or 4KB page. (not a 2M, 1G block)
#define NON_BLOCK (1ULL << 1)

// Memory attributes index field.
#define ATTR_INDX (0b111 << 2)

// Non-secure bit. For memory accesses from Secure state, specifies whether the output
// address is in Secure or Non-secure memory.
#define NS (1ULL << 5)

// Access permission: accessable at EL0.
#define AP_EL0 (1ULL << 6)

// Access permission: read-only.
#define AP_RO (1ULL << 7)

// Shareability: Inner Shareable (otherwise Outer Shareable).
#define INNER (1ULL << 8)

// Shareability: Inner or Outer Shareable (otherwise Non-shareable).
#define SHAREABLE (1ULL << 9)

// The Access flag.
#define AF (1ULL << 10)

// The not global bit.
#define NG (1ULL << 11)

// Indicates that 16 adjacent translation table entries point to contiguous memory regions.
#define CONTIGUOUS (1ULL << 52)

// The Privileged execute-never field. 1 就是禁止，0是允许
#define PXN (1ULL << 53)

// The Execute-never or Unprivileged execute-never field. 1 就是禁止，0是允许
#define UXN (1ULL << 54)

// Next-level attributes in stage 1 VMSAv8-64 Table descriptors:


// 特权读写 AF置位
#define PTE_SH (0b11 << 8) // 对于SMP系统来说,全部设置为Inner-share就可以了
#define PTE_RW (0 << 7)

#define MM_TYPE_BLOCK 0b01
#define MM_TYPE_TABLE 0b11

// 默认页表项 带缓存的memory,inner share
#define PTE_NORMAL_MEMORY (MM_TYPE_BLOCK | PTE_AIDX_MEMORY | PTE_SH | AF | PTE_RW )
// 设备内存页表项
#define PTE_DEVICE_MEMORY (MM_TYPE_BLOCK | PTE_AIDX_DEVICE_nGnRn | PTE_SH | AF | PTE_RW)

#endif