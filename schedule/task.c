#include <task.h>
#include <io.h>
#include <gic.h>
#include <hyper/vcpu.h>
#include <mem/aj_string.h>
#include <sys/sys.h>
#include <spinlock.h>
#include <thread.h>
#include "os_cfg.h"

tcb_t task_list[MAX_TASKS];
cpu_t g_cpu_dec[MAX_TASKS];

// 目前只有首核会添加一个任务，所以task_count不需要锁
uint32_t task_count = 0;

static spinlock_t lock;
static spinlock_t print_lock;
static void switch_context_el(tcb_t *old, tcb_t *new, uint64_t *sp);

extern void switch_context(tcb_t *, tcb_t *);
extern void get_all_sysregs(cpu_sysregs_t *);

tcb_t * create_task(void (*task_func)(), uint64_t stack_top)
{
    if (task_count >= MAX_TASKS)
        return (tcb_t *)0;

    tcb_t *task = &task_list[task_count];
    task->id = task_count;
    task->state = 1;
    task->cpu_info = &g_cpu_dec[task_count];

    
    task->cpu_info->ctx.elr = (uint64_t)task_func; // elr_el1
    task->cpu_info->ctx.spsr = SPSR_EL1_USER;      // spsr_el1
    task->cpu_info->ctx.usp = (uint64_t)(task_func + 0x40000);

    memcpy((void*)( stack_top - sizeof(trap_frame_t) ), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el0_tesk_entry();
    task->ctx.x30 = (uint64_t)el0_tesk_entry;
    task->ctx.sp_el1 = stack_top - sizeof(trap_frame_t);

    task->counter = SYS_TASK_TICK;
    task_count++;

    return task;
}

tcb_t * craete_vm_task(void (*task_func)())
{
    if (task_count >= MAX_TASKS)
        return(tcb_t *)0;

    tcb_t *task = &task_list[task_count];
    task->id = task_count;
    task->state = 1;
    task->cpu_info = &g_cpu_dec[task_count];

    task->cpu_info->ctx.elr = (uint64_t)task_func; // elr_el2
    task->cpu_info->ctx.spsr = SPSR_EL2_VALUE_IRQ;         // spsr_el2
    task->cpu_info->ctx.r[0] = (0x70000000);
    
    static cpu_sysregs_t initial_sysregs;
    // get_all_sysregs(&initial_sysregs);
    // memcpy(&task->cpu_info->sys_reg, &initial_sysregs, sizeof(cpu_sysregs_t));
    task->cpu_info->sys_reg = &cpu_sysregs[task_count];
    task->cpu_info->sys_reg->spsr_el1 = 0x30C50830;

    task->counter = SYS_TASK_TICK;
    task_count++;

    return task;
}

void schedule_init()
{
    spinlock_init(&lock);
    spinlock_init(&print_lock);
    for (int i = 0; i < MAX_TASKS; i++)
    {
        task_list[i].id = -1;
    }
}

void schedule_init_local()
{
    spin_lock(&print_lock);
    struct thread_info *info = current_thread_info();
    info->current_thread = &task_list[info->cpu];
    printf("core %d current task %d\n", info->cpu, ((tcb_t *)info->current_thread)->id);
    task_list[info->cpu].state = RUNNING;
    spin_unlock(&print_lock);
}

void print_current_task_list()
{
    for (int i = 0; i < task_count; i++)
    {
        tcb_t *task = &task_list[i];
        // printf("id: %x, sp: 0x%x, lr: 0x%x\n", task->id, task->ctx.x29, task->ctx.x30);
        printf("id: %x, elr: 0x%x\n", task->id, task->cpu_info->ctx.elr);
    }
    // printf("current task id: %d\n", current_task->id);
    printf("\n");
}



void _schedule(uint64_t *sp)
{
    if (task_count == SMP_NUM)
        return;

    // 找到下一个就绪的任务
    // 这里多个核不能同时计算

    struct thread_info *info = current_thread_info();
    tcb_t *curr = (tcb_t *)info->current_thread;

    uint32_t next_task_id = -1;
    spin_lock(&lock);
    for (int i = 1; i < task_count; i++)
    {
        next_task_id = (curr->id + i) % task_count;
        // 跳过非就绪状态的任务
        if (task_list[next_task_id].state == RUNNING)
        {
            continue;
        }
        else
        {
            task_list[curr->id].state = WAITING;
            task_list[next_task_id].state = RUNNING;
            info->current_thread = &task_list[next_task_id];
            break;
        }
    }
    spin_unlock(&lock);
    if (next_task_id == -1)
    {   
        printf("no task to schedule, current task %d\n", curr->id);
        return;
    }

    tcb_t *next_task = &task_list[next_task_id];
    tcb_t *prev_task = curr;
    printf("core %d switch prev_task %d to next_task %d\n", current_thread_info()->cpu, prev_task->id, next_task->id);

    if (sp == NULL)
        switch_context(prev_task, next_task);
    else
        switch_context_el(prev_task, next_task, sp);
}

void schedule(void)
{
    tcb_t *curr = (tcb_t *)current_thread_info()->current_thread;
    curr->counter = 0;
    _schedule(NULL);
}

void timer_tick_schedule(uint64_t *sp)
{
    struct thread_info * info = current_thread_info();
    tcb_t *curr = (tcb_t *)info->current_thread;
    --curr->counter;

    if (curr->counter > 0)
        return;

    curr->counter = SYS_TASK_TICK;
    // disable_interrupts();
    _schedule(NULL);
    // enable_interrupts();
}

//  vm 相关

extern void restore_sysregs(cpu_sysregs_t *);
extern void save_sysregs(cpu_sysregs_t *);

// 这时候的 curr 已经是下一个任务了
void vm_in()
{
    struct thread_info * info = current_thread_info();
    tcb_t *curr = (tcb_t *)info->current_thread;
    restore_sysregs(curr->cpu_info->sys_reg);
}

void vm_out()
{
    struct thread_info * info = current_thread_info();
    tcb_t *curr = (tcb_t *)info->current_thread;
    save_sysregs(curr->cpu_info->sys_reg);
}

void save_cpu_ctx(trap_frame_t *sp)
{
    tcb_t *curr = (tcb_t *)current_thread_info()->current_thread;
    memcpy(&curr->cpu_info->ctx, sp, sizeof(trap_frame_t));
    curr->cpu_info->pctx = sp;
}

// 这个函数会直接改变 trap frame 里面的内容
void switch_context_el(tcb_t *old, tcb_t *new, uint64_t *sp)
{
    // memcpy(&old->cpu_info->ctx, sp, sizeof(trap_frame_t)); // 保存上下文到vpu dev 中
    memcpy(sp, &new->cpu_info->ctx, sizeof(trap_frame_t)); // 恢复下一个任务的cpu ctx
}
