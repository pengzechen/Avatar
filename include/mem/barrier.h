

#ifndef __BARRIER_H__
#define __BARRIER_H__

#define isb(option) __asm__ __volatile__ ("isb " #option : : : "memory")
#define dsb(option) __asm__ __volatile__ ("dsb " #option : : : "memory")
#define dmb(option) __asm__ __volatile__ ("dmb " #option : : : "memory")


#define mb()		dsb()
#define rmb()		dsb()
#define wmb()		dsb(st)

#endif // __BARRIER_H__