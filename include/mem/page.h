

#ifndef __PAGE_H__
#define __PAGE_H__

#define __packed __attribute__((packed))

#include "avatar_types.h"
#include "barrier.h"

typedef struct __packed
{
    /* 这些字段在所有类型的条目中都使用。 */
    unsigned long valid : 1; /* 有效映射 */
    unsigned long table : 1; /* 在4k映射条目中也等于1 */

    /* 这十个位仅在块条目中使用，在表条目中被忽略。 */
    unsigned long mattr : 4; /* 内存属性 */
    unsigned long read : 1;  /* 读访问 */
    unsigned long write : 1; /* 写访问 */
    unsigned long sh : 2;    /* 共享性 */
    unsigned long af : 1;    /* 访问标志 */
    unsigned long sbz4 : 1;  /* 必须为零 */

    /* 基地址必须对块条目进行适当对齐 */
    unsigned long base : 36; /* 块或下一级表的基地址 */
    unsigned long sbz3 : 4;  /* 必须为零 */

    /* 这七个位仅在块条目中使用，在表条目中被忽略。 */
    unsigned long contig : 1; /* 在16个连续条目中的块 */
    unsigned long sbz2 : 1;   /* 必须为零 */
    unsigned long xn : 1;     /* 不可执行 */
    unsigned long type : 4;   /* 硬件忽略。用于存储p2m类型 */

    unsigned long sbz1 : 5; /* 必须为零 */
} lpae_p2m_t;

typedef union
{
    uint64_t bits;
    lpae_p2m_t p2m;
} lpae_t;

typedef union
{
    struct
    {
        unsigned long is_valid : 1, is_table : 1, ignored1 : 10,
            next_table_addr : 36, reserved : 4, ignored2 : 7,
            PXNTable : 1, // Privileged Execute-never for next level
            XNTable : 1,  // Execute-never for next level
            APTable : 2,  // Access permissions for next level
            NSTable : 1;
    } table;
    struct
    {
        unsigned long is_valid : 1, is_table : 1,
            attr_index : 3, // Memory attributes index
            NS : 1,         // Non-secure
            AP : 2,         // Data access permissions
            SH : 2,         // Shareability
            AF : 1,         // Accesss flag
            nG : 1,         // Not global bit
            reserved1 : 4, nT : 1, reserved2 : 13, pfn : 18,
            reserved3 : 2, GP : 1, reserved4 : 1,
            DBM : 1, // Dirty bit modifier
            Contiguous : 1,
            PXN : 1, // Privileged execute-never
            UXN : 1, // Execute never
            soft_reserved : 4,
            PBHA : 4; // Page based hardware attributes
    } l1_block;
    struct
    {
        unsigned long is_valid : 1, is_table : 1,
            attr_index : 3, // Memory attributes index
            NS : 1,         // Non-secure
            AP : 2,         // Data access permissions
            SH : 2,         // Shareability
            AF : 1,         // Accesss flag
            nG : 1,         // Not global bit
            reserved1 : 4, nT : 1, reserved2 : 4, pfn : 27,
            reserved3 : 2, GP : 1, reserved4 : 1,
            DBM : 1, // Dirty bit modifier
            Contiguous : 1,
            PXN : 1, // Privileged execute-never
            UXN : 1, // Execute never
            soft_reserved : 4,
            PBHA : 4; // Page based hardware attributes
    } l2_block;
    struct
    {
        unsigned long is_valid : 1, is_table : 1,
            attr_index : 3, // Memory attributes index
            NS : 1,         // Non-secure
            AP : 2,         // Data access permissions
            SH : 2,         // Shareability
            AF : 1,         // Accesss flag
            nG : 1,         // Not global bit
            pfn : 36, reserved : 3,
            DBM : 1, // Dirty bit modifier
            Contiguous : 1,
            PXN : 1, // Privileged execute-never
            UXN : 1, // Execute never
            soft_reserved : 4,
            PBHA : 4, // Page based hardware attributes
            ignored : 1;
    } l3_page;
    uint64_t pte;
} pte_t;

// 安全获取缓存行大小的函数声明
size_t get_cacheline_size(void);

#define CTR_EL0_CWG_MASK 0xFF // CWG 字段在 CTR_EL0 中的位掩码

static inline void __clean_dcache_one(const void *addr)
{
    __asm__ __volatile__("dc cvac %0," : : "r"(addr));
}

static inline void __invalidate_dcache_one(const void *addr)
{
    __asm__ __volatile__("dc ivac %0," : : "r"(addr));
}

static inline void __clean_and_invalidate_dcache_one(const void *addr)
{
    __asm__ __volatile__("dc civac, %0" ::"r"(addr));
}

static inline int32_t invalidate_dcache_va_range(const void *p, unsigned long size)
{
    size_t off;
    const void *end = p + size;
    size_t cache_line_size = get_cacheline_size();

    dsb(sy); /* So the CPU issues all writes to the range */

    off = (unsigned long)p % cache_line_size;
    if (off)
    {
        p -= off;
        __clean_and_invalidate_dcache_one(p);
        p += cache_line_size;
        size -= cache_line_size - off;
    }
    off = (unsigned long)end % cache_line_size;
    if (off)
    {
        end -= off;
        size -= off;
        __clean_and_invalidate_dcache_one(end);
    }

    for (; p < end; p += cache_line_size)
        __invalidate_dcache_one(p);

    dsb(sy); /* So we know the flushes happen before continuing */

    return 0;
}

static inline int32_t clean_and_invalidate_dcache_va_range(const void *p, unsigned long size)
{
    const void *end;
    size_t cache_line_size = get_cacheline_size();
    dsb(sy); /* So the CPU issues all writes to the range */
    for (end = p + size; p < end; p += cache_line_size)
        __clean_and_invalidate_dcache_one(p);
    dsb(sy); /* So we know the flushes happen before continuing */
    /* ARM callers assume that dcache_* functions cannot fail. */
    return 0;
}

static inline void flush_tlb(void)
{
    // Data Synchronization Barrier, ensure all previous memory accesses are complete
    __asm__ volatile("dsb sy");

    // Invalidate all TLB entries in the Inner Shareable domain for the current VMID
    __asm__ volatile("tlbi aside1is, xzr");

    // Invalidate all TLB entries for the current VMID
    __asm__ volatile("tlbi alle1is");

    // Data Synchronization Barrier, ensure completion of TLB invalidation
    __asm__ volatile("dsb sy");

    // Instruction Synchronization Barrier, ensure subsequent instructions use new TLB entries
    __asm__ volatile("isb");
}

static inline pte_t *read_ttbr0_el1(void)
{
    uint64_t val;
    asm volatile("mrs %0, ttbr0_el1" : "=r"(val));
    return (pte_t *)(val);
}

#endif // __PAGE_H__