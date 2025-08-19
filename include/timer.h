/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file timer.h
 * @brief Implementation of timer.h
 * @author Avatar Project Team
 * @date 2024
 */



#ifndef __TIMER_H__
#define __TIMER_H__

#include "avatar_types.h"

// CNTFRQ_EL0    （频率寄存器）
static inline uint64_t
read_cntfrq_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

// CNTFRQ_EL0    （频率寄存器）
static inline void
write_cntfrq_el0(uint64_t val)
{
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
static inline void
write_cntp_ctl_el0(uint64_t val)
{
    asm volatile("msr cntp_ctl_el0, %0" : : "r"(val));
}

// CNTP_TVAL_EL0 （定时值寄存器）
static inline void
write_cntp_tval_el0(uint64_t val)
{
    asm volatile("msr cntp_tval_el0, %0" : : "r"(val));
}

// 	CNTP_CVAL_EL0 比较值寄存器
static inline void
write_cntp_cval_el0(uint64_t val)
{
    asm volatile("msr cntp_cval_el0, %0" : : "r"(val));
}

// CNTPCT_EL0    （计数值寄存器）
static inline uint64_t
read_cntpct_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

// ========== Hypervisor Timer (EL2) 寄存器操作 ==========

// CNTHP_CTL_EL2 （Hypervisor Timer 控制寄存器）
static inline void
write_cnthp_ctl_el2(uint64_t val)
{
    asm volatile("msr cnthp_ctl_el2, %0" : : "r"(val));
}

static inline uint64_t
read_cnthp_ctl_el2(void)
{
    uint64_t val;
    asm volatile("mrs %0, cnthp_ctl_el2" : "=r"(val));
    return val;
}

// CNTHP_TVAL_EL2 （Hypervisor Timer 定时值寄存器）
static inline void
write_cnthp_tval_el2(uint64_t val)
{
    asm volatile("msr cnthp_tval_el2, %0" : : "r"(val));
}

static inline uint64_t
read_cnthp_tval_el2(void)
{
    uint64_t val;
    asm volatile("mrs %0, cnthp_tval_el2" : "=r"(val));
    return val;
}

// CNTHP_CVAL_EL2 （Hypervisor Timer 比较值寄存器）
static inline void
write_cnthp_cval_el2(uint64_t val)
{
    asm volatile("msr cnthp_cval_el2, %0" : : "r"(val));
}

static inline uint64_t
read_cnthp_cval_el2(void)
{
    uint64_t val;
    asm volatile("mrs %0, cnthp_cval_el2" : "=r"(val));
    return val;
}

// ========== Virtual Timer Offset (EL2) 寄存器操作 ==========

// CNTVOFF_EL2 （Virtual Timer Offset 寄存器）
static inline void
write_cntvoff_el2(uint64_t val)
{
    asm volatile("msr cntvoff_el2, %0" : : "r"(val));
}

static inline uint64_t
read_cntvoff_el2(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntvoff_el2" : "=r"(val));
    return val;
}

void
timer_init();
void
timer_init_second();

#endif  // __TIMER_H__