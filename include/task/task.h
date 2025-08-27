/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file task.h
 * @brief Implementation of task.h
 * @author Avatar Project Team
 * @date 2024
 */


#ifndef __TASK_H__
#define __TASK_H__

#include "avatar_types.h"
#include "vmm/vcpu.h"
#include "lib/list.h"
#include "os_cfg.h"
#include "thread.h"
#include "pro.h"
#include "vmm/vm.h"

#define IPI_SCHED 2

// 任务入口函数类型定义
typedef void (*entry_t)(void);

#define wfi() __asm__ volatile("wfi" : : : "memory")


// 获取当前任务的宏定义
#define curr_task_el1() ((tcb_t *) (void *) read_tpidr_el0())
#define curr_task_el2() ((tcb_t *) (void *) read_tpidr_el2())
#define curr_task()     (get_el() == 2 ? curr_task_el2() : curr_task_el1())

typedef struct _cpu_sysregs cpu_sysregs_t;

#pragma pack(1)
typedef struct _contex_t
{
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;        // Stack Frame Pointer
    uint64_t x30;        // Link register (the address to return)
    uint64_t tpidr_elx;  // "Thread ID" Register
    uint64_t sp_elx;
} contex_t;
#pragma pack()

typedef struct _cpu_t
{
    cpu_ctx_t      ctx;
    cpu_ctx_t     *pctx;  // 指向trap的栈，可以修改restore的数据
    cpu_sysregs_t *sys_reg;
} cpu_t;

extern cpu_t vcpu[];

typedef enum _task_state_t
{
    TASK_STATE_CREATE = 1,  // 刚分配完 TCB，还没进入任何队列
    TASK_STATE_READY,       // 已进入 ready 队列，等待调度
    TASK_STATE_RUNNING,
    TASK_STATE_WAITING,   // 睡眠状态（sleep tick 到期后可转 READY）
    TASK_STATE_WAIT_IRQ,  // 等待中断
} task_state_t;

#pragma pack(1)
typedef struct _tcb_t
{
    contex_t ctx;
    cpu_t   *cpu_info;

    uint64_t pgdir;  // 页表基址
    uint64_t sp;     // 栈地址

    task_state_t state;
    uint32_t     affinity;

    int32_t task_id;  // 任务ID
    int32_t remaining_ticks;

    int32_t  sleep_ticks;
    uint32_t reserved;


    list_node_t run_node;    // 运行相关结点
    list_node_t wait_node;   // 等待队列
    list_node_t sleep_node;  // 睡眠队列 not used
    list_node_t all_node;    // 所有队列结点

    list_node_t        process_node;  // 属于哪个进程
    struct _process_t *curr_pro;      // 当前进程

    list_node_t   vm_node;  // 属于哪个虚拟机
    struct _vm_t *curr_vm;  // 当前虚拟机
} tcb_t;
#pragma pack()

typedef struct cpu_scheduler
{
    uint32_t cpu_id;  // CPU ID

    spinlock_t sched_lock;

    tcb_t *current_task;  // 当前运行的任务

    tcb_t idle_task;  // idle任务
    cpu_t idle_cpu;   // idle任务cpu

    // 就绪队列（简单的循环链表）
    list_t ready_list;

    // 睡眠队列（按唤醒时间排序的链表）
    list_t sleep_list;

    // 统计信息
    uint64_t total_switches;  // 总切换次数
    uint64_t total_ticks;     // 总tick数
    uint64_t idle_ticks;      // idle时间
} cpu_scheduler_t;


typedef struct _task_manager_t
{
    list_t task_list;  // 所有已创建任务的队列

    // list_t ready_list[SMP_NUM];  // 就绪队列
    // list_t sleep_list[SMP_NUM];  // 延时队列 - 改为per-CPU
    // tcb_t  idle_task[SMP_NUM];   // 空闲任务
    // cpu_t  idle_cpu[SMP_NUM];    // cpu 上下文

    cpu_scheduler_t sched[SMP_NUM];

} task_manager_t;

void
timer_tick_schedule(uint64_t *);
void
print_current_task_list();

static inline void
flush(void)
{
    // 确保页表写入已完成（比如 TTBR0_EL1 已写好）
    __asm__ volatile("dsb ish");  // Data Synchronization Barrier，Inner Shareable

    // 清除所有EL1 TLB项，适用于多核 inner shareable 域
    __asm__ volatile("tlbi vmalle1is");  // Invalidate all TLB entries for EL1 (Inner Shareable)

    // 等待 TLB 刷新完成
    __asm__ volatile("dsb ish");

    // 确保后续指令看到最新的 TLB 状态
    __asm__ volatile("isb");
}

static inline uint32_t
can_run_on_core(uint32_t affinity, uint32_t coreid)
{
    if (coreid >= 32) {
        // 超出范围，priority 只有32位
        return false;
    }
    return (affinity & (1U << coreid)) != 0;
}

tcb_t *
alloc_tcb();

tcb_t *
create_task(entry_t task_func, uint64_t, uint32_t);

tcb_t *
create_vm_task(entry_t task_func, uint64_t stack_top, uint32_t affinity, uint64_t dtb_addr);

void
reset_task(tcb_t *task, entry_t task_func, uint64_t stack_top, uint32_t affinity);

void
schedule_init();
void
task_manager_init(void);
task_manager_t *
get_task_manager();
void
schedule_init_local(tcb_t *task, void *new_sp);

void
task_add_to_readylist_tail(tcb_t *task);
void
task_add_to_readylist_head(tcb_t *task);
void
task_add_to_readylist_tail_remote(tcb_t *task, uint32_t core_id);
void
task_add_to_readylist_head_remote(tcb_t *task, uint32_t core_id);
void
task_remove_from_readylist(tcb_t *task);
void
task_remove_from_readylist_remote(tcb_t *task, uint32_t core_id);

tcb_t *
get_idle();
void
el1_idle_init();  // 初始化空闲任务
void
el2_idle_init();  // 初始化空闲任务
uint64_t
get_idle_sp_top();

void
schedule();

// 睡眠队列相关函数
void
task_set_sleep(tcb_t *task, uint64_t ticks);
void
task_set_wakeup(tcb_t *task);
void
task_set_wakeup_percpu(tcb_t *task, uint32_t core_id);

// 系统调用
void
sys_sleep_tick(uint64_t ms);

#endif  // __TASK_H__