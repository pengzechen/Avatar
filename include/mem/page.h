

#ifndef __PAGE_H__
#define __PAGE_H__

#define __packed __attribute__((packed))

#include "aj_types.h"
#include "barrier.h"

typedef struct __packed
{
    /* 这些字段在所有类型的条目中都使用。 */
    unsigned long valid : 1; /* 有效映射 */
    unsigned long table : 1; /* 在4k映射条目中也等于1 */

    /* 这十个位仅在块条目中使用，在表条目中被忽略。 */
    unsigned long ai : 3;   /* 属性索引 */
    unsigned long ns : 1;   /* 非安全 */
    unsigned long user : 1; /* 用户可见 */
    unsigned long ro : 1;   /* 只读 */
    unsigned long sh : 2;   /* 共享性 */
    unsigned long af : 1;   /* 访问标志 */
    unsigned long ng : 1;   /* 非全局 */

    /* 基地址必须对块条目进行适当对齐 */
    unsigned long long base : 36; /* 块或下一级表的基地址 */
    unsigned long sbz : 4;        /* 必须为零 */

    /* 这七个位仅在块条目中使用，在表条目中被忽略。 */
    unsigned long contig : 1; /* 在16个连续条目中的块 */
    unsigned long pxn : 1;    /* 特权级不可执行 */
    unsigned long xn : 1;     /* 不可执行 */
    unsigned long avail : 4;  /* 硬件忽略 */

    /* 这五个位仅在表条目中使用，在块条目中被忽略。 */
    unsigned long pxnt : 1; /* 特权级不可执行 */
    unsigned long xnt : 1;  /* 不可执行 */
    unsigned long apt : 2;  /* 访问权限 */
    unsigned long nst : 1;  /* 非安全 */
} lpae_pt_t;

/* p2m表的布局几乎相同，但一些权限和缓存控制位的布局不同（或缺失） */
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
    unsigned long long base : 36; /* 块或下一级表的基地址 */
    unsigned long sbz3 : 4;       /* 必须为零 */

    /* 这七个位仅在块条目中使用，在表条目中被忽略。 */
    unsigned long contig : 1; /* 在16个连续条目中的块 */
    unsigned long sbz2 : 1;   /* 必须为零 */
    unsigned long xn : 1;     /* 不可执行 */
    unsigned long type : 4;   /* 硬件忽略。用于存储p2m类型 */

    unsigned long sbz1 : 5; /* 必须为零 */
} lpae_p2m_t;

/*
 * Walk is the common bits of p2m and pt entries which are needed to
 * simply walk the table (e.g. for debug).
 */
typedef struct __packed
{
    /* These are used in all kinds of entry. */
    unsigned long valid : 1; /* Valid mapping */
    unsigned long table : 1; /* == 1 in 4k map entries too */

    unsigned long pad2 : 10;

    /* The base address must be appropriately aligned for Block entries */
    unsigned long long base : 36; /* Base address of block or next table */

    unsigned long pad1 : 16;
} lpae_walk_t;

typedef union
{
    uint64_t bits;
    lpae_pt_t pt;
    lpae_p2m_t p2m;
    lpae_walk_t walk;
} lpae_t;

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

extern size_t cacheline_bytes;

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

static inline int invalidate_dcache_va_range(const void *p, unsigned long size)
{
    size_t off;
    const void *end = p + size;

    dsb(sy); /* So the CPU issues all writes to the range */

    off = (unsigned long)p % cacheline_bytes;
    if (off)
    {
        p -= off;
        __clean_and_invalidate_dcache_one(p);
        p += cacheline_bytes;
        size -= cacheline_bytes - off;
    }
    off = (unsigned long)end % cacheline_bytes;
    if (off)
    {
        end -= off;
        size -= off;
        __clean_and_invalidate_dcache_one(end);
    }

    for (; p < end; p += cacheline_bytes)
        __invalidate_dcache_one(p);

    dsb(sy); /* So we know the flushes happen before continuing */

    return 0;
}

static inline int clean_and_invalidate_dcache_va_range(const void *p, unsigned long size)
{
    const void *end;
    dsb(sy); /* So the CPU issues all writes to the range */
    for (end = p + size; p < end; p += cacheline_bytes)
        __clean_and_invalidate_dcache_one(p);
    dsb(sy); /* So we know the flushes happen before continuing */
    /* ARM callers assume that dcache_* functions cannot fail. */
    return 0;
}

#endif // __PAGE_H__