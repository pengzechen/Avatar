#include <task/task.h>
#include <io.h>
#include <gic.h>
#include <vmm/vcpu.h>
#include <lib/aj_string.h>
#include <sys/sys.h>
#include <spinlock.h>
#include <thread.h>
#include "os_cfg.h"
#include <mem/page.h>
#include <mem/mem.h>

// 下面三个变量仅仅在 alloc_tcb 和 free_tcb 使用
tcb_t g_task_dec[MAX_TASKS];
cpu_t g_cpu_dec[MAX_TASKS];

static spinlock_t lock;
static spinlock_t print_lock;
extern void switch_context(tcb_t *, tcb_t *);
extern void switch_context_el2(tcb_t *, tcb_t *);

static task_manager_t task_manager;
static tcb_t *task_next_run(void);

void task_remove_from_readylist(tcb_t *task);
void task_add_to_readylist_tail(tcb_t *task);

void task_set_wakeup(tcb_t *task);
void task_set_sleep(tcb_t *task, uint64_t ticks);

tcb_t *alloc_tcb()
{
    static uint32_t task_count = 1; // 这个数字会不停累加下去
    for (uint32_t i = 0; i < MAX_TASKS; ++i)
    {
        if (g_task_dec[i].task_id == 0)
        { // 找到空闲的 TCB
            tcb_t *task = &g_task_dec[i];
            task->cpu_info = &g_cpu_dec[i];
            task->cpu_info->sys_reg = &cpu_sysregs[i];
            task->cpu_info->pctx = NULL; // 初始化为 NULL

            task->task_id = task_count++; // 使用空闲位置，设置有效的 ID

            task->ctx.tpidr_elx = (uint64_t)task;
            task->ctx.sp_elx = 0LL;

            task->state = TASK_STATE_CREATE;
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
    return NULL; // 如果没有空闲的 TCB，返回 NULL
}

// TODO：还没考虑好什么什么状态的 task 可以被 free
void free_tcb(tcb_t *task)
{
    if (task == NULL)
        return;

    // 每个核删除自己的节点
    uint32_t core_id = get_current_cpu_id();
    spin_lock(&task_manager.lock);
    list_delete(&task_manager.ready_list[core_id], &task->run_node);

    // 从任务管理的任务列表中移除任务
    list_delete(&task_manager.task_list, &task->all_node);
    list_delete(&task_manager.sleep_list, &task->wait_node);
    spin_unlock(&task_manager.lock);

    memset(task, 0, sizeof(tcb_t));
}

tcb_t *create_task(void (*task_func)(), uint64_t stack_top, uint32_t priority)
{
    tcb_t *task = alloc_tcb();
    if (task == NULL)
    {
        logger_warn("create task failed, no free tcb!\n");
        return NULL; // 如果没有空闲的 TCB，返回 NULL
    }
    task->counter = SYS_TASK_TICK;
    task->priority = priority;
    list_insert_last(&task_manager.task_list, &task->all_node);

    task->cpu_info->ctx.elr = (uint64_t)task_func; // elr_el1
    task->cpu_info->ctx.spsr = SPSR_EL0_EL0t;      // spsr_el1
    task->cpu_info->ctx.usp = (uint64_t)(task_func + 0x4000);

    memcpy((void *)(stack_top - sizeof(trap_frame_t)), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el0_task_entry();
    task->ctx.x30 = (uint64_t)el0_task_entry;
    task->ctx.x29 = stack_top - sizeof(trap_frame_t);
    task->ctx.sp_elx = stack_top - sizeof(trap_frame_t);
    task->sp = (stack_top - PAGE_SIZE * 2);

    return task;
}

tcb_t *create_vm_task(void (*task_func)(), uint64_t stack_top, uint32_t priority)
{
    tcb_t *task = alloc_tcb();
    if (task == NULL)
    {
        logger_warn("create task failed, no free tcb!\n");
        return NULL; // 如果没有空闲的 TCB，返回 NULL
    }
    task->counter = SYS_TASK_TICK;
    task->priority = priority;
    list_insert_last(&task_manager.task_list, &task->all_node);

    task->cpu_info->ctx.elr = (uint64_t)task_func; // elr_el2
    task->cpu_info->ctx.spsr = SPSR_VALUE;         // spsr_el2
    task->cpu_info->ctx.r[0] = (0x70000000);
    task->cpu_info->sys_reg->spsr_el1 = 0x30C50830;

    memcpy((void *)(stack_top - sizeof(trap_frame_t)), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void guest_entry();
    task->ctx.x30 = (uint64_t)guest_entry;
    task->ctx.sp_elx = stack_top - sizeof(trap_frame_t); // el2 的栈

    return task;
}

void reset_task(tcb_t *task, void (*task_func)(), uint64_t stack_top, uint32_t priority)
{
    task->counter = SYS_TASK_TICK;
    task->priority = priority;

    task->cpu_info->ctx.elr = (uint64_t)task_func; // elr_el1
    task->cpu_info->ctx.spsr = SPSR_EL0_EL0t;      // spsr_el1
    task->cpu_info->ctx.usp = (uint64_t)(task_func + 0x4000);

    memcpy((void *)(stack_top - sizeof(trap_frame_t)), &task->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el0_task_entry();
    task->ctx.x30 = (uint64_t)el0_task_entry;
    task->ctx.x29 = stack_top - sizeof(trap_frame_t);
    task->ctx.sp_elx = stack_top - sizeof(trap_frame_t);
    task->sp = (stack_top - PAGE_SIZE * 2);
}

void schedule_init()
{
    spinlock_init(&lock);
    spinlock_init(&print_lock);
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
    logger("core %d current task %d\n", get_current_cpu_id(), task->task_id);
    spin_unlock(&print_lock);
}

void print_current_task_list()
{
    logger("\n    task all list:\n");
    list_node_t *curr = list_first(&task_manager.task_list);
    while (curr)
    {
        list_node_t *next = list_node_next(curr);
        tcb_t *task = list_node_parent(curr, tcb_t, all_node);
        logger("id: %llx, elr: 0x%llx, priority: %d, state: %d\n", task->task_id, task->cpu_info->ctx.elr, task->priority, task->state);
        curr = next;
    }
    logger("\n");
}

void schedule()
{
    tcb_t *curr = NULL;
    if (get_el() == 2)
        curr = (tcb_t *)(void *)read_tpidr_el2();
    else
        curr = (tcb_t *)(void *)read_tpidr_el0();

    uint32_t core_id = get_current_cpu_id();
    tcb_t *next_task = task_next_run();
    tcb_t *prev_task = curr;
    if (next_task == curr)
    {
        char buffer[512];
        int offset = 0;

        offset += my_snprintf(buffer + offset, sizeof(buffer) - offset,
                             "[warning]: core: %d, n = c task 0x%llx, id: %d. readylist: ",
                             get_current_cpu_id(), curr, curr->task_id);

        list_node_t *iter = list_first(&task_manager.ready_list[core_id]);
        while (iter && offset < sizeof(buffer) - 50) // 保留足够空间
        {
            tcb_t *task = list_node_parent(iter, tcb_t, run_node);
            offset += my_snprintf(buffer + offset, sizeof(buffer) - offset,
                                 "(id :%d, state: %d), ", task->task_id, task->state);
            iter = list_node_next(iter);
        }

        logger_task_debug("%s\n", buffer);
        return;
    }

    logger_task_debug("core %d switch prev_task %d to next_task %d\n",
        get_current_cpu_id(), prev_task->task_id, next_task->task_id);

    // logger("next_task page dir: 0x%llx\n", next_task->pgdir);
    next_task->state = TASK_STATE_RUNNING;

    if (get_el() == 1)
    {
        uint64_t val = virt_to_phys(next_task->pgdir);
        asm volatile("msr ttbr0_el1, %[x]" : : [x] "r"(val));
        dsb_sy();
        isb();
        tlbi_vmalle1();
        dsb_sy();
        isb();
        switch_context(prev_task, next_task);
    }
    else
    {
        switch_context_el2(prev_task, next_task);
    }
}

void timer_tick_schedule(uint64_t *sp)
{
    tcb_t *curr_task = NULL;
    if (get_el() == 2)
        curr_task = (tcb_t *)(void *)read_tpidr_el2();
    else
        curr_task = (tcb_t *)(void *)read_tpidr_el0();

    // 睡眠处理
    spin_lock(&task_manager.lock);
    list_node_t *curr = list_first(&task_manager.sleep_list);
    spin_unlock(&task_manager.lock);
    while (curr)
    {
        list_node_t *next = list_node_next(curr);

        tcb_t *task = list_node_parent(curr, tcb_t, run_node);
        if (--task->sleep_ticks == 0)
        {
            // logger("task %d sleep time arrive\n", task->id);
            task_set_wakeup(task);
            task_add_to_readylist_tail(task); // 此时 task 状态会设置为 READY
        }
        curr = next;
    }

    // 时间片耗尽
    if (--curr_task->counter <= 0)
    {
        if (curr_task != &task_manager.idle_task[get_current_cpu_id()])
        {
            curr_task->counter = SYS_TASK_TICK;
            task_add_to_readylist_tail(curr_task); // 会设置状态为 READY
        }
        else
        {
            curr_task->counter = SYS_TASK_TICK / 5;
        }
        schedule();
    }
}

//  vm 相关
// 这时候的 curr 已经是下一个任务了
void vm_in()
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    extern void restore_sysregs(cpu_sysregs_t *);
    extern void gicc_restore_core_state();
    extern void vgic_try_inject_pending(tcb_t *task);
    extern void vtimer_core_restore(tcb_t *task);
    restore_sysregs(curr->cpu_info->sys_reg);

    // 先修改内存中的值
    if (!curr->curr_vm)
        return;
    vgic_try_inject_pending(curr);

    // 恢复虚拟定时器状态
    vtimer_core_restore(curr);

    // 内存恢复到硬件
    gicc_restore_core_state();
}

void vm_out()
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    extern void save_sysregs(cpu_sysregs_t *);
    extern void gicc_save_core_state();
    extern void vtimer_core_save(tcb_t *task);
    save_sysregs(curr->cpu_info->sys_reg);

    if (!curr->curr_vm)
        return;
    // 保存虚拟定时器状态
    vtimer_core_save(curr);

    gicc_save_core_state();
}

void save_cpu_ctx(trap_frame_t *sp)
{
    tcb_t *curr = NULL;
    if (get_el() == 2)
    {
        curr = (tcb_t *)read_tpidr_el2();
    }
    else
    {
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

static uint8_t idle_task_stack[SMP_NUM][IDLE_STACK_SIZE] __attribute__((aligned(4096)));

void idle_task_el1()
{
    while (1)
    {
        wfi();
        // __asm__ __volatile__("msr daifclr, #2" : : : "memory");
        // for (int32_t i = 0; i < 100000000; i++);
        // logger("current el: %d, idle task\n", get_el());
    }
}

tcb_t *get_idle()
{
    uint64_t core_id = get_current_cpu_id();
    return &task_manager.idle_task[core_id];
}

uint64_t get_idle_sp_top(void)
{
    uint64_t core_id = get_current_cpu_id();
    return (uint64_t)&idle_task_stack[core_id][IDLE_STACK_SIZE];
}

void el1_idle_init()
{
    uint64_t core_id = get_current_cpu_id();
    tcb_t *idel = &task_manager.idle_task[core_id];
    idel->task_id = -(core_id + 1);
    idel->counter = 10;
    idel->cpu_info = &task_manager.idle_cpu[core_id];
    idel->cpu_info->ctx.elr = (uint64_t)idle_task_el1; // elr_el1
    idel->cpu_info->ctx.spsr = SPSR_EL1_EL1h;        // spsr_el1
    idel->cpu_info->ctx.usp = 0;
    idel->pgdir = get_kpgdir(); // pgdir

    uint64_t stack_top = get_idle_sp_top();
    memcpy((void *)(stack_top - sizeof(trap_frame_t)), &idel->cpu_info->ctx, sizeof(trap_frame_t));
    extern void el1_task_entry();
    idel->ctx.x30 = (uint64_t)el1_task_entry;
    idel->ctx.sp_elx = stack_top - sizeof(trap_frame_t);
    idel->ctx.tpidr_elx = (uint64_t)idel;
}

void el2_idle_init()
{
    uint64_t core_id = get_current_cpu_id();
    tcb_t *idel = &task_manager.idle_task[core_id];
    idel->task_id = -(core_id + 1);
    idel->counter = 10;
    idel->cpu_info = &task_manager.idle_cpu[core_id];
    idel->cpu_info->ctx.elr = (uint64_t)idle_task_el1; // elr_el2
    idel->cpu_info->ctx.spsr = SPSR_EL2_EL2h;        // spsr_el2
    idel->cpu_info->ctx.usp = 0;
    idel->pgdir = get_kpgdir(); // pgdir

    uint64_t stack_top = get_idle_sp_top();
    memcpy((void *)(stack_top - sizeof(trap_frame_t)), &idel->cpu_info->ctx, sizeof(trap_frame_t));
    logger_warn("core: %d, idle task stack top: 0x%llx\n", core_id, stack_top);
    extern void el2_tesk_entry();
    idel->ctx.x30 = (uint64_t)el2_tesk_entry;
    idel->ctx.sp_elx = stack_top - sizeof(trap_frame_t);
    idel->ctx.tpidr_elx = (uint64_t)idel;
}

// 初始化任务管理器，初始化空闲任务
void task_manager_init(void)
{
    // 各队列初始化
    for (int32_t i = 0; i < SMP_NUM; i++)
    {
        list_init(&task_manager.ready_list[i]);
    }
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);

    spinlock_init(&task_manager.lock);
}

task_manager_t *get_task_manager()
{
    return &task_manager;
}

void task_add_to_readylist_tail_remote(tcb_t *task, uint32_t core_id)
{
    if (core_id >= SMP_NUM)
        logger_error("error: wrong core id\n");
    // logger("core id: %d\n", core_id);
    spin_lock(&task_manager.lock);
    if (task != &task_manager.idle_task[core_id])
    {
        list_insert_last(&task_manager.ready_list[core_id], &task->run_node);
        task->state = TASK_STATE_READY;
    }
    spin_unlock(&task_manager.lock);
}

// ============ 就绪队列相关操作 ============

// 将任务插入就绪队列 (后插),设置为就绪状态
void task_add_to_readylist_tail(tcb_t *task)
{
    uint64_t core_id = get_current_cpu_id();
    // spin_lock(&task_manager.lock);
    if (task != &task_manager.idle_task[core_id])
    {
        list_insert_last(&task_manager.ready_list[core_id], &task->run_node);
        task->state = TASK_STATE_READY;
    }
    // spin_unlock(&task_manager.lock);
}

// 将任务从就绪队列移除
void task_remove_from_readylist(tcb_t *task)
{
    uint64_t core_id = get_current_cpu_id();
    // spin_lock(&task_manager.lock);
    if (task != &task_manager.idle_task[core_id])
    {
        list_delete(&task_manager.ready_list[core_id], &task->run_node);
    }
    // spin_unlock(&task_manager.lock);
}

// 获取下一将要运行的任务 (第一个 ready 节点)
// 并且把它放回就绪队列的尾部
static tcb_t *task_next_run(void)
{
    uint64_t core_id = get_current_cpu_id();

    list_node_t *iter = list_first(&task_manager.ready_list[core_id]);
    while (iter)
    {
        tcb_t *task = list_node_parent(iter, tcb_t, run_node);
        if (task->state == TASK_STATE_READY)
        {
            task_remove_from_readylist(task);
            return task;
        }
        else
        {
            logger_error("task %d state is not ready, state: %d\n", task->task_id, task->state);
        }
        iter = list_node_next(iter);
    }

    return &task_manager.idle_task[core_id];
}

// ============ 延时队列相关操作 ============

// 将任务加入延时队列
void task_set_sleep(tcb_t *task, uint64_t ticks)
{
    if (ticks <= 0)
    {
        return;
    }

    task->sleep_ticks = ticks;
    task->state = TASK_STATE_WAITING;

    spin_lock(&task_manager.lock);
    list_insert_last(&task_manager.sleep_list, &task->run_node);
    spin_unlock(&task_manager.lock);
}

// 将任务从延时队列移除
void task_set_wakeup(tcb_t *task)
{
    spin_lock(&task_manager.lock);
    list_delete(&task_manager.sleep_list, &task->run_node);
    task->state = TASK_STATE_READY;
    spin_unlock(&task_manager.lock);
}

void sys_sleep_tick(uint64_t ms)
{
    // 至少延时1个tick
    if (ms < SYS_TASK_TICK)
    {
        ms = SYS_TASK_TICK;
    }

    // 从就绪队列移除，加入睡眠队列
    tcb_t *curr = (tcb_t *)(void *)read_tpidr_el0();
    task_remove_from_readylist(curr);
    task_set_sleep(curr, ms / 10);
    // logger("sleep %d ms, tick: %d\n", ms, curr->sleep_ticks);

    // 进行一次调度
    schedule();
}

void task_yield(void)
{
    tcb_t *curr_task;
    if (get_el() == 2)
        curr_task = (tcb_t *)(void *)read_tpidr_el2();
    else
        curr_task = (tcb_t *)(void *)read_tpidr_el0();

    // idle 任务不需要让出 CPU（它本来就是在等别人用 CPU）
    if (curr_task == &task_manager.idle_task[get_current_cpu_id()])
        return;

    // 放回 ready 队尾
    task_add_to_readylist_tail(curr_task);

    // 重置时间片（可选，取决于你是否要立即重新分配时间）
    curr_task->counter = SYS_TASK_TICK;

    // 切换任务
    schedule();
}

// vwfi
void task_wait_for_irq(void)
{
    tcb_t *curr_task;
    if (get_el() == 2)
        curr_task = (tcb_t *)(void *)read_tpidr_el2();
    else
        curr_task = (tcb_t *)(void *)read_tpidr_el0();
    curr_task->state = TASK_STATE_WAIT_IRQ;
    task_remove_from_readylist(curr_task);
    schedule();  // 调度出去

    // 唤醒点类似这样
    /*
    if (task->state == TASK_STATE_WAIT_IRQ)
    {
        task_add_to_readylist_tail(task);
    }
    */
}
