#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include "../avatar_types.h"

/**
 * 原子比较并交换操作 (Compare and Swap with Acquire semantics)
 * @param p: 指向要操作的变量的指针
 * @param old: 期望的旧值
 * @param newv: 要设置的新值
 * @return: 返回原始值；如果返回值等于old，则交换成功
 */
static inline int
atomic_cmpxchg_acquire(volatile int *p, int old, int newv)
{
    int prev, tmp;
    __asm__ __volatile__("1: ldaxr %w0, [%2]\n"       // 加载当前值到prev，带acquire语义
                         "   cmp   %w0, %w3\n"        // 比较当前值与期望值
                         "   b.ne  2f\n"              // 如果不相等，跳到结束
                         "   stlxr %w1, %w4, [%2]\n"  // 尝试存储新值，带release语义
                         "   cbnz  %w1, 1b\n"         // 如果存储失败，重试
                         "2:"
                         : "=&r"(prev), "=&r"(tmp)
                         : "r"(p), "r"(old), "r"(newv)
                         : "cc", "memory");
    return prev;
}

/**
 * 原子递减并返回新值 (Atomic decrement with Release semantics)
 * @param p: 指向要操作的变量的指针
 * @return: 返回递减后的值
 */
static inline int
atomic_dec_return_release(volatile int *p)
{
    int val, tmp, result;
    __asm__ __volatile__("1: ldxr %w0, [%3]\n"        // 加载当前值
                         "   sub  %w2, %w0, #1\n"     // 计算递减后的值
                         "   stlxr %w1, %w2, [%3]\n"  // 尝试存储新值，带release语义
                         "   cbnz %w1, 1b\n"          // 如果存储失败，重试
                         : "=&r"(val), "=&r"(tmp), "=&r"(result)
                         : "r"(p)
                         : "cc", "memory");
    return result;
}

/**
 * 原子递增并返回新值 (Atomic increment with Release semantics)
 * @param p: 指向要操作的变量的指针
 * @return: 返回递增后的值
 */
static inline int
atomic_inc_return_release(volatile int *p)
{
    int val, tmp, result;
    __asm__ __volatile__("1: ldxr %w0, [%3]\n"        // 加载当前值
                         "   add  %w2, %w0, #1\n"     // 计算递增后的值
                         "   stlxr %w1, %w2, [%3]\n"  // 尝试存储新值，带release语义
                         "   cbnz %w1, 1b\n"          // 如果存储失败，重试
                         : "=&r"(val), "=&r"(tmp), "=&r"(result)
                         : "r"(p)
                         : "cc", "memory");
    return result;
}

/**
 * 原子加法并返回新值 (Atomic add with Release semantics)
 * @param p: 指向要操作的变量的指针
 * @param val: 要加的值
 * @return: 返回加法后的值
 */
static inline int
atomic_add_return_release(volatile int *p, int val)
{
    int old, tmp, result;
    __asm__ __volatile__("1: ldxr %w0, [%3]\n"        // 加载当前值
                         "   add  %w2, %w0, %w4\n"    // 计算加法后的值
                         "   stlxr %w1, %w2, [%3]\n"  // 尝试存储新值，带release语义
                         "   cbnz %w1, 1b\n"          // 如果存储失败，重试
                         : "=&r"(old), "=&r"(tmp), "=&r"(result)
                         : "r"(p), "r"(val)
                         : "cc", "memory");
    return result;
}

/**
 * 原子交换操作 (Atomic exchange with Acquire-Release semantics)
 * @param p: 指向要操作的变量的指针
 * @param newv: 要设置的新值
 * @return: 返回原始值
 */
static inline int
atomic_xchg_acq_rel(volatile int *p, int newv)
{
    int prev, tmp;
    __asm__ __volatile__("1: ldaxr %w0, [%2]\n"       // 加载当前值，带acquire语义
                         "   stlxr %w1, %w3, [%2]\n"  // 尝试存储新值，带release语义
                         "   cbnz  %w1, 1b\n"         // 如果存储失败，重试
                         : "=&r"(prev), "=&r"(tmp)
                         : "r"(p), "r"(newv)
                         : "cc", "memory");
    return prev;
}

/**
 * 原子加载操作 (Atomic load with Acquire semantics)
 * @param p: 指向要读取的变量的指针
 * @return: 返回读取的值
 */
static inline int
atomic_load_acquire(volatile int *p)
{
    int val;
    __asm__ __volatile__("ldar %w0, [%1]\n" : "=r"(val) : "r"(p) : "memory");
    return val;
}

/**
 * 原子存储操作 (Atomic store with Release semantics)
 * @param p: 指向要写入的变量的指针
 * @param val: 要写入的值
 */
static inline void
atomic_store_release(volatile int *p, int val)
{
    __asm__ __volatile__("stlr %w1, [%0]\n" : : "r"(p), "r"(val) : "memory");
}

#endif  // __ATOMIC_H__
