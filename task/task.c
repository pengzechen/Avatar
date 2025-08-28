/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file task.c
 * @brief Implementation of task.c
 * @author Avatar Project Team
 * @date 2024
 */

#include "task/task.h"
#include "io.h"
#include "gic.h"
#include "vmm/vcpu.h"
#include "lib/avatar_string.h"
#include "sys/sys.h"
#include "spinlock.h"
#include "thread.h"
#include "os_cfg.h"
#include "mem/earlypage.h"
#include "mem/page.h"
#include "mem/mem.h"
#include "lib/avatar_assert.h"
#include "exception.h"

// 下面三个变量仅仅在 alloc_tcb 和 free_tcb 使用
tcb_t          g_task_dec[MAX_TASKS];
cpu_t          g_cpu_dec[MAX_TASKS];
static uint8_t idle_task_stack[SMP_NUM][IDLE_STACK_SIZE] __attribute__((aligned(4096)));

extern void
switch_context(tcb_t *, tcb_t *);
extern void
switch_context_el2(tcb_t *, tcb_t *);

static task_manager_t task_manager;

static tcb_t *
task_next_run(void);
void
task_set_wakeup(tcb_t *task);
void
task_set_sleep(tcb_t *task, uint64_t ticks);

//
tcb_t *
alloc_tcb()
{
    static uint32_t task_count = 1;  // 这个数字会不停累加下去
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        if (g_task_dec[i].task_id == 0) {  // 找到空闲的 TCB
            tcb_t *task             = &g_task_dec[i];
            task->cpu_info          = &g_cpu_dec[i];
            task->cpu_info->sys_reg = &cpu_sysregs[i];
            task->cpu_info->pctx    = NULL;  // 初始化为 NULL

            task->task_id = task_count++;  // 使用空闲位置，设置有效的 ID

            task->ctx.tpidr_elx = (uint64_t) task;
            task->ctx.sp_elx    = 0LL;

            task->state   = TASK_STATE_CREATE;
            task->curr_vm = NULL;

            // 初始化任务的链表节点
            list_node_init(&task->all_node);
            list_node_init(&task->run_node);
            list_node_init(&task->wait_node);

            list_node_init(&task->process_node);
            list_node_init(&task->vm_node);
            return task;
        }
    }
    return NULL;  // 如果没有空闲的 TCB，返回 NULL
}

void
free_tcb(tcb_t *task)
{
    if (task == NULL)
        return;

    // 每个核删除自己的节点
    uint32_t core_id = get_current_cpu_id();

    // 从任务管理的任务列表中移除任务
    list_delete(&task_manager.task_list, &task->all_node);

    memset(task, 0, sizeof(tcb_t));
}

tcb_t *
create_task(entry_t task_func, uint64_t stack_top, uint32_t affinity)
{
    tcb_t *task = alloc_tcb();
    if (task == NULL) {
        logger_warn("create task failed, no free tcb!\n");
        return NULL;  // 如果没有空闲的 TCB，返回 NULL
    }
    task->remaining_ticks = SYS_TASK_TICK;
    task->affinity        = affinity;
    list_insert_last(&task_manager.task_list, &task->all_node);

    task->cpu_info->ctx.elr  = (uint64_t) task_func;  // elr_el1
    task->cpu_info->ctx.spsr = SPSR_EL0_EL0t;         // spsr_el1
    task->cpu_info->ctx.usp  = (uint64_t) (task_func + 0x4000);

    memcpy((void *) (stack_top - sizeof(trap_frame_t)), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el0_task_entry();
    task->ctx.x30    = (uint64_t) el0_task_entry;
    task->ctx.x29    = stack_top - sizeof(trap_frame_t);
    task->ctx.sp_elx = stack_top - sizeof(trap_frame_t);
    task->sp         = (stack_top - PAGE_SIZE * 2);

    return task;
}

tcb_t *
create_vm_task(entry_t task_func, uint64_t stack_top, uint32_t affinity, uint64_t dtb_addr)
{
    tcb_t *task = alloc_tcb();
    if (task == NULL) {
        logger_warn("create task failed, no free tcb!\n");
        return NULL;  // 如果没有空闲的 TCB，返回 NULL
    }
    task->remaining_ticks = SYS_TASK_TICK;
    task->affinity        = affinity;
    list_insert_last(&task_manager.task_list, &task->all_node);

    task->cpu_info->ctx.elr           = (uint64_t) task_func;  // elr_el2
    task->cpu_info->ctx.spsr          = SPSR_VALUE;            // spsr_el2
    task->cpu_info->ctx.r[0]          = dtb_addr;              // Linux 通过 x0 传递 DTB 地址
    task->cpu_info->sys_reg->spsr_el1 = 0x30C50830;

    memcpy((void *) (stack_top - sizeof(trap_frame_t)), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void guest_entry();
    task->ctx.x30    = (uint64_t) guest_entry;
    task->ctx.sp_elx = stack_top - sizeof(trap_frame_t);  // el2 的栈

    return task;
}

void
reset_task(tcb_t *task, entry_t task_func, uint64_t stack_top, uint32_t affinity)
{
    task->remaining_ticks = SYS_TASK_TICK;
    task->affinity        = affinity;

    task->cpu_info->ctx.elr  = (uint64_t) task_func;  // elr_el1
    task->cpu_info->ctx.spsr = SPSR_EL0_EL0t;         // spsr_el1
    task->cpu_info->ctx.usp  = (uint64_t) (task_func + 0x4000);

    memcpy((void *) (stack_top - sizeof(trap_frame_t)), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el0_task_entry();
    task->ctx.x30    = (uint64_t) el0_task_entry;
    task->ctx.x29    = stack_top - sizeof(trap_frame_t);
    task->ctx.sp_elx = stack_top - sizeof(trap_frame_t);
    task->sp         = (stack_top - PAGE_SIZE * 2);
}

void
schedule_init()
{
    logger_info("schedule init done\n");
}

void
schedule_init_local(tcb_t *task, void *new_sp)
{
    irq_install(IPI_SCHED, schedule);
    gic_enable_int(IPI_SCHED, 1);
    gic_set_ipriority(IPI_SCHED, 0x0);  // 最高优先级

    if (get_el() == 2) {
        task->state = TASK_STATE_RUNNING;
        write_tpidr_el2((uint64_t) task);
    } else {
        task->state = TASK_STATE_RUNNING;
        write_tpidr_el0((uint64_t) task);
    }
    logger("core %d current task %d\n", get_current_cpu_id(), task->task_id);
}

void
print_current_task_list()
{
    logger("\n    task all list:\n");
    list_node_t *curr = list_first(&task_manager.task_list);
    while (curr) {
        list_node_t *next = list_node_next(curr);
        tcb_t       *task = list_node_parent(curr, tcb_t, all_node);
        logger("id: %llx, elr: 0x%llx, affinity: %d, state: %d\n",
               task->task_id,
               task->cpu_info->ctx.elr,
               task->affinity,
               task->state);
        curr = next;
    }
    logger("\n");
}

// =============== sched =====================

void
scheduler_init(cpu_scheduler_t *sched, int32_t i)
{
    memset(sched, 0, sizeof(cpu_scheduler_t));

    sched->cpu_id = i;

    spinlock_init(&sched->sched_lock);
    sched->current_task = NULL;

    list_init(&sched->ready_list);
    list_init(&sched->sleep_list);
}

cpu_scheduler_t *
get_scheduler()
{
    uint64_t core_id = get_current_cpu_id();
    return &task_manager.sched[core_id];
}


// 获取(该cpu)下一将要运行的任务 (第一个 ready 节点)
// 从就绪队列中移除该任务
static tcb_t *
task_next_run(void)
{
    cpu_scheduler_t *schde      = get_scheduler();
    tcb_t           *task_torun = NULL;

    list_node_t *iter = list_first(&schde->ready_list);
    while (iter) {
        tcb_t *task = list_node_parent(iter, tcb_t, run_node);
        if (task->state == TASK_STATE_READY) {
            task_torun = task;
            if (task != &schde->idle_task) {
                list_node_t *node = list_delete(&schde->ready_list, &task->run_node);
                avatar_assert(node != NULL);
            }
            break;
        } else {
            logger_error("task %d state is not ready, state: %d\n", task->task_id, task->state);
        }
        iter = list_node_next(iter);
    }

    if (task_torun != NULL) {
        return task_torun;
    } else {
        return &schde->idle_task;
    }
}

// 将当前任务切换为 task_net_run
void
schedule()
{
    tcb_t *curr = curr_task();

    cpu_scheduler_t *schde     = get_scheduler();
    tcb_t           *next_task = task_next_run();
    tcb_t           *prev_task = curr;
    if (next_task == curr) {
        char buffer[512];
        int  offset = 0;

        offset += my_snprintf(buffer + offset,
                              sizeof(buffer) - offset,
                              "[warning]: core: %d, n = c task 0x%llx, id: %d. readylist: ",
                              get_current_cpu_id(),
                              curr,
                              curr->task_id);

        list_node_t *iter = list_first(&schde->ready_list);
        while (iter && offset < sizeof(buffer) - 50)  // 保留足够空间
        {
            tcb_t *task = list_node_parent(iter, tcb_t, run_node);
            offset += my_snprintf(buffer + offset,
                                  sizeof(buffer) - offset,
                                  "(id :%d, state: %d), ",
                                  task->task_id,
                                  task->state);
            iter = list_node_next(iter);
        }

        logger_task_debug("%s\n", buffer);
        return;
    }

    logger_task_debug("core %d switch prev_task %d to next_task %d\n",
                      get_current_cpu_id(),
                      prev_task->task_id,
                      next_task->task_id);

    // logger("next_task page dir: 0x%llx\n", next_task->pgdir);
    next_task->state = TASK_STATE_RUNNING;

    if (get_el() == 1) {
        uint64_t val = virt_to_phys(next_task->pgdir);
        asm volatile("msr ttbr0_el1, %[x]" : : [x] "r"(val));
        dsb_sy();
        isb();
        tlbi_vmalle1();
        dsb_sy();
        isb();
        switch_context(prev_task, next_task);
    } else {
        switch_context_el2(prev_task, next_task);
    }
}

// 查看(该cpu)延时队列和counter
void
timer_tick_schedule(uint64_t *sp)
{
    tcb_t *curr_task = curr_task();

    // 睡眠处理 - 只处理当前CPU的睡眠队列
    // 注意：在中断上下文中，当前CPU的中断已被禁用，且每个CPU只访问自己的睡眠队列
    // 因此不需要加锁保护
    cpu_scheduler_t *schde      = get_scheduler();
    bool             has_wakeup = false;  // 标记是否有任务被唤醒
    // logger_task_debug("tick arrived!\n");

    list_node_t *curr = list_first(&schde->sleep_list);
    while (curr) {
        list_node_t *next = list_node_next(curr);

        tcb_t *task = list_node_parent(curr, tcb_t, run_node);
        if (--task->sleep_ticks <= 0) {
            logger_task_debug("task %d sleep time arrive\n", task->task_id);
            task_set_wakeup(task);  // 从当前CPU的睡眠队列移除
            // 任务在当前CPU睡眠，唤醒后直接加入当前CPU的就绪队列头部（高优先级）
            task_add_to_readylist_head(task);
            task->remaining_ticks = SYS_TASK_TICK;
            has_wakeup            = true;  // 有任务被唤醒
        }
        curr = next;
    }

    // 时间片处理
    bool need_schedule = false;
    if (--curr_task->remaining_ticks <= 0) {
        if (curr_task != get_idle()) {
            curr_task->remaining_ticks = SYS_TASK_TICK;
            task_add_to_readylist_tail(curr_task);  // 时间片耗尽，放到队尾
        }
        need_schedule = true;  // 时间片耗尽需要调度
    }

    // 调度决策：有任务唤醒或时间片耗尽都需要调度
    if (has_wakeup || need_schedule) {
        schedule();
    }
}

//  vm 相关
// 这时候的 curr 已经是下一个任务了
void
vm_in()
{
    tcb_t      *curr = curr_task_el2();
    extern void restore_sysregs(cpu_sysregs_t *);
    extern void gicc_restore_core_state();
    extern void vgic_try_inject_pending(tcb_t * task);
    extern void vtimer_core_restore(tcb_t * task);

    // 先修改内存中的值
    if (!curr->curr_vm)
        return;

    restore_sysregs(curr->cpu_info->sys_reg);

    vgic_try_inject_pending(curr);

    // 恢复虚拟定时器状态
    vtimer_core_restore(curr);

    // 内存恢复到硬件
    gicc_restore_core_state();
}

void
vm_out()
{
    tcb_t      *curr = curr_task_el2();
    extern void save_sysregs(cpu_sysregs_t *);
    extern void gicc_save_core_state();
    extern void vtimer_core_save(tcb_t * task);
    if (!curr->curr_vm)
        return;

    save_sysregs(curr->cpu_info->sys_reg);

    // 保存虚拟定时器状态
    vtimer_core_save(curr);

    gicc_save_core_state();
}

// 保存栈上的内容到task cpu中
void
save_cpu_ctx(trap_frame_t *sp)
{
    tcb_t *curr = curr_task();
    memcpy(&curr->cpu_info->ctx, sp, sizeof(trap_frame_t));
    curr->cpu_info->pctx = sp;
}

// ================= 任务管理 =================

void
idle_task()
{
    while (1) {
        wfi();
        // __asm__ __volatile__("msr daifclr, #2" : : : "memory");
        // for (int32_t i = 0; i < 100000000; i++);
        // logger("current el: %d, idle task\n", get_el());
    }
}

tcb_t *
get_idle()
{
    cpu_scheduler_t *schde = get_scheduler();
    return &schde->idle_task;
}

uint64_t
get_idle_sp_top(void)
{
    uint64_t core_id = get_current_cpu_id();
    return (uint64_t) &idle_task_stack[core_id][IDLE_STACK_SIZE];
}

void
el1_idle_init()
{
    cpu_scheduler_t *schde   = get_scheduler();
    uint64_t         core_id = get_current_cpu_id();

    tcb_t *idle              = &schde->idle_task;
    idle->task_id            = -(core_id + 1);
    idle->remaining_ticks    = -1;
    idle->cpu_info           = &schde->idle_cpu;
    idle->cpu_info->ctx.elr  = (uint64_t) idle_task;  // elr_el1
    idle->cpu_info->ctx.spsr = SPSR_EL1_EL1h;         // spsr_el1
    idle->cpu_info->ctx.usp  = 0;
    idle->pgdir              = get_kpgdir();  // pgdir

    uint64_t stack_top = get_idle_sp_top();
    memcpy((void *) (stack_top - sizeof(trap_frame_t)), &idle->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el1_task_entry();
    idle->ctx.x30       = (uint64_t) el1_task_entry;
    idle->ctx.sp_elx    = stack_top - sizeof(trap_frame_t);
    idle->ctx.tpidr_elx = (uint64_t) idle;
}

void
el2_idle_init()
{
    cpu_scheduler_t *schde   = get_scheduler();
    uint64_t         core_id = get_current_cpu_id();


    tcb_t *idle              = &schde->idle_task;
    idle->task_id            = -(core_id + 1);
    idle->remaining_ticks    = -1;
    idle->cpu_info           = &schde->idle_cpu;
    idle->cpu_info->ctx.elr  = (uint64_t) idle_task;  // elr_el2
    idle->cpu_info->ctx.spsr = SPSR_EL2_EL2h;         // spsr_el2
    idle->cpu_info->ctx.usp  = 0;
    idle->pgdir              = get_kpgdir();  // pgdir

    uint64_t stack_top = get_idle_sp_top();
    memcpy((void *) (stack_top - sizeof(trap_frame_t)), &idle->cpu_info->ctx, sizeof(trap_frame_t));
    logger_warn("core: %d, idle task stack top: 0x%llx\n", core_id, stack_top);
    extern void el2_tesk_entry();
    idle->ctx.x30       = (uint64_t) el2_tesk_entry;
    idle->ctx.sp_elx    = stack_top - sizeof(trap_frame_t);
    idle->ctx.tpidr_elx = (uint64_t) idle;
}

// 初始化任务管理器，初始化空闲任务
void
task_manager_init(void)
{
    // 各队列初始化
    for (int32_t i = 0; i < SMP_NUM; i++) {
        scheduler_init(&task_manager.sched[i], i);
    }
    list_init(&task_manager.task_list);
}

task_manager_t *
get_task_manager()
{
    return &task_manager;
}


// =========================================
// ============ 就绪队列相关操作 ============

void
task_add_to_readylist_tail_remote(tcb_t *task, uint32_t core_id)
{
    if (core_id >= SMP_NUM)
        logger_error("error: wrong core id\n");
    cpu_scheduler_t *sched = &task_manager.sched[core_id];
    if (task != &sched->idle_task) {
        list_insert_last(&sched->ready_list, &task->run_node);
        task->state = TASK_STATE_READY;
    }
}

void
task_add_to_readylist_head_remote(tcb_t *task, uint32_t core_id)
{
    if (core_id >= SMP_NUM)
        logger_error("error: wrong core id\n");
    cpu_scheduler_t *sched = &task_manager.sched[core_id];
    if (task != &sched->idle_task) {
        list_insert_first(&sched->ready_list, &task->run_node);
        task->state = TASK_STATE_READY;
    }
}


// 将任务插入就绪队列 (后插),设置为就绪状态
void
task_add_to_readylist_tail(tcb_t *task)
{
    uint64_t core_id = get_current_cpu_id();
    task_add_to_readylist_tail_remote(task, core_id);
}

// 将任务插入就绪队列 (前插),设置为就绪状态
void
task_add_to_readylist_head(tcb_t *task)
{
    uint64_t core_id = get_current_cpu_id();
    task_add_to_readylist_head_remote(task, core_id);
}


// ============ 延时队列相关操作 ============
// =========================================

// 将任务加入延时队列 - 加入到当前CPU的睡眠队列
void
task_set_sleep(tcb_t *task, uint64_t ticks)
{
    if (ticks <= 0) {
        return;
    }

    task->sleep_ticks = ticks;
    task->state       = TASK_STATE_WAITING;

    cpu_scheduler_t *schde = get_scheduler();
    list_insert_last(&schde->sleep_list, &task->run_node);
}

// 将任务从延时队列移除 - 从指定CPU的睡眠队列移除
void
task_set_wakeup_remote(tcb_t *task, uint32_t core_id)
{
    cpu_scheduler_t *schde = &task_manager.sched[core_id];
    list_node_t     *node  = list_delete(&schde->sleep_list, &task->run_node);
    avatar_assert(node != NULL);
}

// 将任务从延时队列移除 - 兼容性函数，从当前CPU的睡眠队列移除
void
task_set_wakeup(tcb_t *task)
{
    uint32_t core_id = get_current_cpu_id();
    task_set_wakeup_remote(task, core_id);
}

void
sys_sleep_tick(uint64_t ms)
{
    // 至少延时1个tick
    if (ms < SYS_TASK_TICK) {
        ms = SYS_TASK_TICK;
    }

    // 从就绪队列移除，加入睡眠队列
    tcb_t *curr = curr_task_el1();
    task_set_sleep(curr, ms / 10);
    logger_task_debug("sleep %d ms, tick: %d\n", ms, curr->sleep_ticks);

    // 进行一次调度
    schedule();
}

void
task_yield(void)
{
    tcb_t *curr_task = curr_task();

    // 放回 ready 队尾
    task_add_to_readylist_tail(curr_task);

    // 重置时间片（可选，取决于你是否要立即重新分配时间）
    curr_task->remaining_ticks = SYS_TASK_TICK;

    // 切换任务
    schedule();
}

// vwfi
void
task_wait_for_irq(void)
{
    tcb_t *curr_task = curr_task();
    curr_task->state = TASK_STATE_WAIT_IRQ;
    schedule();  // 调度出去

    // 唤醒点类似这样
    /*
    if (task->state == TASK_STATE_WAIT_IRQ)
    {
        task_add_to_readylist_tail(task);
    }
    */
}
