

#include "task/mutex.h"
#include "task/task.h"
#include "io.h"
#include "thread.h"
#include "gic.h"
#include "mem/barrier.h"

// 初始化互斥锁
void mutex_init(mutex_t *mutex)
{
    // 将互斥锁的锁定计数器初始化为0
    mutex->locked_count = 0;
    // 将互斥锁的所有者初始化为空指针
    mutex->owner = (tcb_t *)0;
    // 初始化互斥锁的等待列表
    list_init(&mutex->wait_list);

    spinlock_init(&mutex->lock);
}

// 定义一个函数，用于锁定互斥锁
void mutex_lock(mutex_t *m)
{
    tcb_t *curr = curr_task_el1();

    // 1) 快速路径：0 -> 1，acquire
    if (atomic_cmpxchg_acquire(&m->locked_count, 0, 1) == 0)
    {
        WRITE_ONCE(m->owner, curr);
        return;
    } // 如果互斥锁已经被当前线程锁定

    // 2) 递归持有
    if (READ_ONCE(m->owner) == curr)
    {
        // 简化：递增即可（当前核独占）
        WRITE_ONCE(m->locked_count, READ_ONCE(m->locked_count) + 1);
        return;
    }

    // 3) 慢路径：入等待队列并阻塞
    spin_lock(&m->lock);

    // 双检：避免窗口期在拿锁
    if (atomic_cmpxchg_acquire(&m->locked_count, 0, 1) == 0)
    {
        WRITE_ONCE(m->owner, curr);
        spin_unlock(&m->lock);
        return;
    }
    list_insert_last(&m->wait_list, &curr->wait_node);

    spin_unlock(&m->lock);

    // logger("core: %d, mutex locked by other task, current task (id: %d, affinity: %d) yield!\n",
    //     get_current_cpu_id(), curr->task_id, curr->affinity);

    // 调度其他线程
    schedule();
}

// 解锁互斥锁
void mutex_unlock(mutex_t *m)
{
    tcb_t *curr = curr_task_el1();

    if (READ_ONCE(m->owner) != curr)
    {
        return;
    }

    // 递归计数 -1，release 语义
    if (atomic_dec_return_release(&m->locked_count) > 0)
        return;

    // 走到这里：完全释放（locked_count == 0）
    spin_lock(&m->lock);

    list_node_t *node = list_delete_first(&m->wait_list);
    if (!node)
    {
        // 无等待者：清空 owner，彻底解锁
        WRITE_ONCE(m->owner, NULL);
        spin_unlock(&m->lock);
        return;
    }

    bool local_sched = false;
    int32_t target = -1;
    tcb_t *task = list_node_parent(node, tcb_t, wait_node);

    WRITE_ONCE(m->locked_count, 1);
    WRITE_ONCE(m->owner, task);

    task_add_to_readylist_head_remote(task, task->affinity - 1);
    dsb(sy);

    if (task->affinity - 1 == get_current_cpu_id())
        local_sched = true;
    else
        target = task->affinity - 1;

    spin_unlock(&m->lock);

    // logger("core: %d, task %d unlock mutex for task %d (affinity: %d)\n",
    //     get_current_cpu_id(), curr->task_id, task->task_id, task->affinity);

    if (local_sched)
    {
        schedule();
    }
    else
    {
        gic_ipi_send_single(IPI_SCHED, target);
        // logger("core: %d, send IPI to core: %d\n", get_current_cpu_id(), target);
    }
}
