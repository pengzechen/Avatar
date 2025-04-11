#include <task.h>
#include <io.h>
#include <gic.h>
#include <hyper/vcpu.h>
#include <mem/aj_string.h>
#include <sys/sys.h>
#include <spinlock.h>
#include <thread.h>
#include "os_cfg.h"

tcb_t g_task_dec[MAX_TASKS];
cpu_t g_cpu_dec[MAX_TASKS];
uint32_t task_count = 0;

static spinlock_t lock;
static spinlock_t print_lock;
static void switch_context_el(tcb_t *old, tcb_t *new, uint64_t *sp);
extern void switch_context(tcb_t *, tcb_t *);
extern void switch_context_el2(tcb_t *, tcb_t *);
extern void get_all_sysregs(cpu_sysregs_t *);
extern uint64_t get_el();
/*
 *  分配一个 tcb 块，设置id，状态为 CREATE
 */
tcb_t *alloc_tcb()
{
    if (task_count >= MAX_TASKS)
        return (tcb_t *)0;
    tcb_t *task = &g_task_dec[task_count];
    task->cpu_info = &g_cpu_dec[task_count];
    task->cpu_info->sys_reg = &cpu_sysregs[task_count];
    task->id = task_count;
    task->ctx.tpidr_elx = (uint64_t)task;
    task->state = TASK_STATE_CREATE;
    task_count++;
    return task;
}

tcb_t *create_task(void (*task_func)(), uint64_t stack_top)
{
    tcb_t *task = alloc_tcb();
    task->counter = SYS_TASK_TICK;

    task->cpu_info->ctx.elr = (uint64_t)task_func; // elr_el1
    task->cpu_info->ctx.spsr = SPSR_EL1_USER;      // spsr_el1
    task->cpu_info->ctx.usp = (uint64_t)(task_func + 0x40000);

    memcpy((void *)(stack_top - sizeof(trap_frame_t)), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el0_tesk_entry();
    task->ctx.x30 = (uint64_t)el0_tesk_entry;
    task->ctx.sp_elx = stack_top - sizeof(trap_frame_t);

    return task;
}

tcb_t *craete_vm_task(void (*task_func)(), uint64_t stack_top)
{
    tcb_t *task = alloc_tcb();
    task->counter = SYS_TASK_TICK;

    task->cpu_info->ctx.elr = (uint64_t)task_func; // elr_el2
    task->cpu_info->ctx.spsr = SPSR_VALUE; // spsr_el2
    task->cpu_info->ctx.r[0] = (0x70000000);
    task->cpu_info->sys_reg->spsr_el1 = 0x30C50830;

    memcpy((void *)(stack_top - sizeof(trap_frame_t)), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void guest_entry();
    task->ctx.x30 = (uint64_t)guest_entry;
    task->ctx.sp_elx = stack_top - sizeof(trap_frame_t);   // el2 的栈

    return task;
}

void schedule_init()
{
    spinlock_init(&lock);
    spinlock_init(&print_lock);
    for (int i = 0; i < MAX_TASKS; i++)
    {
        g_task_dec[i].id = -1;
    }
}

void schedule_init_local(tcb_t *task, void *new_sp)
{
    spin_lock(&print_lock);
    if (get_el() == 2)
    {
        task->state = TASK_STATE_RUNNING;
        write_tpidr_el2((uint64_t)task);
    }
    else
    {
        task->state = TASK_STATE_RUNNING;
        write_tpidr_el0((uint64_t)task);
    }
    printf("core %d current task %d\n", get_current_cpu_id(), task->id);
    spin_unlock(&print_lock);
}

void print_current_task_list()
{
    for (int i = 0; i < task_count; i++)
    {
        tcb_t *task = &g_task_dec[i];
        printf("id: %x, elr: 0x%x\n", task->id, task->cpu_info->ctx.elr);
    }
    printf("\n");
}

void _schedule(uint64_t *sp)
{
    if (task_count == SMP_NUM)
        return;

    tcb_t *curr = NULL;
    if (get_el() == 2)
        curr = (tcb_t *)(void*)read_tpidr_el2();
    else
        curr = (tcb_t *)(void *)read_tpidr_el0();

    uint32_t next_task_id = -1;
    spin_lock(&lock);
    for (int i = 1; i < task_count; i++)
    {
        next_task_id = (curr->id + i) % task_count;
        // 跳过非就绪状态的任务
        if (g_task_dec[next_task_id].state == TASK_STATE_RUNNING)
        {
            continue;
        }
        else
        {
            g_task_dec[curr->id].state = TASK_STATE_WAITING;
            g_task_dec[next_task_id].state = TASK_STATE_RUNNING;
            break;
        }
    }
    spin_unlock(&lock);
    if (next_task_id == -1)
    {
        printf("no task to schedule, current task %d\n", curr->id);
        return;
    }

    tcb_t *next_task = &g_task_dec[next_task_id];
    tcb_t *prev_task = curr;
    printf("core %d switch prev_task %d to next_task %d\n", get_current_cpu_id(), prev_task->id, next_task->id);

    if (get_el() == 1)
        switch_context(prev_task, next_task);
    else
        switch_context_el2(prev_task, next_task);
}

void timer_tick_schedule(uint64_t *sp)
{
    tcb_t *curr = NULL;
    if (get_el() == 2)
        curr = (tcb_t *)(void *)read_tpidr_el2();
    else
        curr = (tcb_t *)(void *)read_tpidr_el0();

    --curr->counter;

    if (curr->counter > 0)
        return;

    curr->counter = SYS_TASK_TICK;

    _schedule(sp);
}

//  vm 相关
extern void restore_sysregs(cpu_sysregs_t *);
extern void save_sysregs(cpu_sysregs_t *);

// 这时候的 curr 已经是下一个任务了
void vm_in()
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    restore_sysregs(curr->cpu_info->sys_reg);
}

void vm_out()
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    save_sysregs(curr->cpu_info->sys_reg);
}

void save_cpu_ctx(trap_frame_t *sp)
{
    tcb_t *curr = NULL;
    if (get_el() == 2) {
        curr = (tcb_t *)read_tpidr_el2();
    } else {
        curr = (tcb_t *)(void *)read_tpidr_el0();
    }
    
    memcpy(&curr->cpu_info->ctx, sp, sizeof(trap_frame_t));
    curr->cpu_info->pctx = sp;
}

// 这个函数会直接改变 trap frame 里面的内容
void switch_context_el(tcb_t *old, tcb_t *new, uint64_t *sp)
{
    memcpy(sp, &new->cpu_info->ctx, sizeof(trap_frame_t)); // 恢复下一个任务的cpu ctx
}
