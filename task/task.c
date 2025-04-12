#include <task/task.h>
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
extern void switch_context(tcb_t *, tcb_t *);
extern void switch_context_el2(tcb_t *, tcb_t *);


static task_manager_t task_manager;
static tcb_t * task_next_run (void);
void task_set_block (tcb_t *task);
void task_set_ready (tcb_t *task);

void task_set_wakeup(tcb_t *task);
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

    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);
    list_insert_last(&task_manager.task_list, &task->all_node);
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
    printf("\n    task all list:\n");
    list_node_t * curr = list_first(&task_manager.task_list);
    while (curr) {
        list_node_t * next = list_node_next(curr);
        tcb_t *task = list_node_parent(curr, tcb_t, all_node);
        printf("id: %x, elr: 0x%x\n", task->id, task->cpu_info->ctx.elr);
        curr = next;
    }
    printf("\n");
}

void schedule()
{
    tcb_t *curr = NULL;
    if (get_el() == 2)
        curr = (tcb_t *)(void*)read_tpidr_el2();
    else
        curr = (tcb_t *)(void *)read_tpidr_el0();

    tcb_t *next_task = task_next_run();
    tcb_t *prev_task = curr;
    if (next_task == curr) {
        // printf("[warning]: next task is current task 0x%x, id: %d\n    ready list:\n", curr, curr->id);
        list_node_t * iter = list_first(&task_manager.ready_list);
        while (iter) {
            list_node_t * next = list_node_next(iter);
            tcb_t * task = list_node_parent(iter, tcb_t, run_node);
            // printf("    task :%d\n", task->id);
            iter = next;
        }
        next_task = &task_manager.idle_task;
        // return;
    }

    task_set_block(next_task);
    task_set_ready(next_task);

    // printf("core %d switch prev_task %d to next_task %d\n", 
    //     get_current_cpu_id(), prev_task->id, next_task->id);

    if (get_el() == 1)
        switch_context(prev_task, next_task);
    else
        switch_context_el2(prev_task, next_task);
}

void timer_tick_schedule(uint64_t *sp)
{
    tcb_t *curr_task = NULL;
    if (get_el() == 2)
        curr_task = (tcb_t *)(void *)read_tpidr_el2();
    else
        curr_task = (tcb_t *)(void *)read_tpidr_el0();

    // 睡眠处理
    list_node_t * curr = list_first(&task_manager.sleep_list);
    while (curr) {
        list_node_t * next = list_node_next(curr);

        tcb_t * task = list_node_parent(curr, tcb_t, run_node);
        if (--task->sleep_ticks == 0) {
            // printf("task %d sleep time arrive\n", task->id);
            task_set_wakeup(task);
            task_set_ready(task);
        }
        curr = next;
    }

    if (--curr_task->counter == 0) {
        curr_task->counter = SYS_TASK_TICK;
    }
    
    schedule();
}

//  vm 相关
// 这时候的 curr 已经是下一个任务了
void vm_in()
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    extern void restore_sysregs(cpu_sysregs_t *);
    restore_sysregs(curr->cpu_info->sys_reg);
}

void vm_out()
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    extern void save_sysregs(cpu_sysregs_t *);
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


// ================= 任务管理 =================

static uint8_t idle_task_stack[4096] __attribute__((aligned(4096)));

void idle_task_el1() {
    while (1)
    {
        wfi();
        // __asm__ __volatile__("msr daifclr, #2" : : : "memory");
        // for (int i = 0; i < 100000000; i++);
        // printf("current el: %d, idle task\n", get_el());
    }
}

void el1_idle_init()
{
    tcb_t *idel = &task_manager.idle_task;
    idel->id = -2;
    idel->counter = 10;
    idel->cpu_info = &task_manager.idle_cpu;
    idel->cpu_info->ctx.elr = (uint64_t)idle_task_el1; // elr_el1
    idel->cpu_info->ctx.spsr = SPSR_EL1_KERNEL;        // spsr_el1
    idel->cpu_info->ctx.usp = 0;
    

    uint64_t stack_top = (uint64_t)idle_task_stack + sizeof(idle_task_stack);
    memcpy((void *)(stack_top - sizeof(trap_frame_t)), &idel->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el0_tesk_entry();
    idel->ctx.x30 = (uint64_t)el0_tesk_entry;
    idel->ctx.sp_elx = stack_top - sizeof(trap_frame_t);
    idel->ctx.tpidr_elx = (uint64_t)idel;
}

// 初始化任务管理器，初始化空闲任务
void task_manager_init(void) {
    // 各队列初始化
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);

    el1_idle_init(); // 初始化空闲任务
}

// 将任务插入就绪队列 (后插)
void task_set_ready(tcb_t *task)
{
    if (task != &task_manager.idle_task)
    {
        list_insert_last(&task_manager.ready_list, &task->run_node);
        task->state = TASK_STATE_READY;
    }
}

// 将任务从就绪队列移除
void task_set_block(tcb_t *task)
{
    if (task != &task_manager.idle_task)
    {
        list_delete(&task_manager.ready_list, &task->run_node);
    }
}

// 获取下一将要运行的任务 (第一个节点)
static tcb_t *task_next_run(void)
{
    // 如果没有任务，则运行空闲任务
    if (list_count(&task_manager.ready_list) == 0)
    {
        return &task_manager.idle_task;
    }

    // 普通任务
    list_node_t *task_node = list_first(&task_manager.ready_list);
    return list_node_parent(task_node, tcb_t, run_node);
}

// 将任务加入延时队列
void task_set_sleep(tcb_t *task, uint64_t ticks)
{
    if (ticks <= 0)
    {
        return;
    }

    task->sleep_ticks = ticks;
    task->state = TASK_STATE_WAITING;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}

// 将任务从延时队列移除
void task_set_wakeup(tcb_t *task)
{
    list_delete(&task_manager.sleep_list, &task->run_node);
}

void sys_sleep_tick (uint64_t ms) {
    // 至少延时1个tick
    if (ms < SYS_TASK_TICK) {
        ms = SYS_TASK_TICK;
    }

    // 从就绪队列移除，加入睡眠队列
    tcb_t * curr = (tcb_t *)(void *)read_tpidr_el0();
    task_set_block(curr);
    task_set_sleep(curr, ms / 10);
    // printf("sleep %d ms, tick: %d\n", ms, curr->sleep_ticks);
    
    // 进行一次调度
    schedule();
}