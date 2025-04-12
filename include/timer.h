

#ifndef __TIMER_H__
#define  __TIMER_H__

#include "aj_types.h"



// CNTFRQ_EL0    （频率寄存器）
static inline uint64_t read_cntfrq_el0(void) {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

// CNTFRQ_EL0    （频率寄存器）
static inline void write_cntfrq_el0(uint64_t val) {
    asm volatile("msr cntfrq_el0, %0" : : "r"(val));
}

// CNTP_CTL_EL0  （控制寄存器）
/*
CNTP_CTL_EL0  （控制寄存器）
ENABLE (bit 0): 启用定时器
0: 禁用定时器
1: 启用定时器
IMASK (bit 1): 中断屏蔽
0: 使能中断
1: 屏蔽中断
ISTATUS (bit 2): 中断状态 (只读)
0: 未触发中断
1: 已触发中断
*/
static inline void write_cntp_ctl_el0(uint64_t val) {
    asm volatile("msr cntp_ctl_el0, %0" : : "r"(val));
}



// CNTP_TVAL_EL0 （定时值寄存器）
static inline void write_cntp_tval_el0(uint64_t val) {
    asm volatile("msr cntp_tval_el0, %0" : : "r"(val));
}

// 	CNTP_CVAL_EL0 比较值寄存器
static inline void write_cntp_cval_el0(uint64_t val) {
    asm volatile("msr cntp_cval_el0, %0" : : "r"(val));
}

// CNTPCT_EL0    （计数值寄存器）
static inline uint64_t read_cntpct_el0(void) {
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}


void timer_init();
void timer_init_second();


#endif // __TIMER_H__