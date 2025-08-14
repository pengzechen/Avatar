

#ifndef __BARRIER_H__
#define __BARRIER_H__

#define isb(option) __asm__ __volatile__("isb " #option : : : "memory")
#define dsb(option) __asm__ __volatile__("dsb " #option : : : "memory")
#define dmb(option) __asm__ __volatile__("dmb " #option : : : "memory")

#define tlbi_vmalle1() __asm__ __volatile__("tlbi vmalle1" ::: "memory")

// 常用封装
#define dsb_sy() dsb(sy)

#define mb() dsb()
#define rmb() dsb()
#define wmb() dsb(st)

// 原子读写宏，用于确保编译器不会优化内存访问
#ifndef READ_ONCE
#define READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))
#endif
#ifndef WRITE_ONCE
#define WRITE_ONCE(x, v) do { (*(volatile __typeof__(x) *)&(x)) = (v); } while (0)
#endif

// 包含原子操作定义
#include "mem/atomic.h"

#endif // __BARRIER_H__