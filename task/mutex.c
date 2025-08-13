

#include "task/mutex.h"
#include "task/task.h"
#include "io.h"
#include "thread.h"
#include "gic.h"

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
void mutex_lock(mutex_t *mutex)
{
    tcb_t *curr = (tcb_t *)(void *)read_tpidr_el0();
    // 如果互斥锁没有被锁定
    if (mutex->locked_count == 0)
    {
        // 将互斥锁的锁定计数设置为1
        mutex->locked_count = 1;
        // 将互斥锁的所有者设置为当前线程控制块
        mutex->owner = curr;
        // 如果互斥锁已经被当前线程锁定
    }
    else if (mutex->owner == curr)
    {
        // 将互斥锁的锁定计数加1
        mutex->locked_count++;
        // 如果互斥锁已经被其他线程锁定
    }
    else
    {
        spin_lock(&mutex->lock);
        list_insert_last(&mutex->wait_list, &curr->wait_node);
        spin_unlock(&mutex->lock);

        logger("core: %d, mutex locked by other task, current task (id: %d, priority: %d) yield!\n",
            get_current_cpu_id(), curr->task_id, curr->priority);

        // 调度其他线程
        schedule();
    }
}

// 解锁互斥锁
void mutex_unlock(mutex_t *mutex)
{
    tcb_t *curr = (tcb_t *)(void *)read_tpidr_el0();
    // 如果当前线程是互斥锁的拥有者
    if (mutex->owner == curr)
    {
        // 互斥锁的锁定计数减一
        if (--mutex->locked_count == 0)
        {
            // 将互斥锁的拥有者置为空
            mutex->owner = (tcb_t *)0;

            // 如果等待队列中有线程
            if (list_count(&mutex->wait_list))
            {
                bool local_sched = false;
                int32_t target = -1;
                
                spin_lock(&mutex->lock);
                list_node_t *task_node = list_delete_first(&mutex->wait_list);
                tcb_t *task = list_node_parent(task_node, tcb_t, wait_node);

                task_add_to_readylist_head_remote(task, task->priority - 1);
                
                if (task->priority - 1 == get_current_cpu_id())
                    local_sched = true;
                else 
                    target = task->priority - 1;

                // 互斥锁的锁定计数置为1
                mutex->locked_count = 1;
                // 将互斥锁的拥有者置为该线程
                mutex->owner = task;
                spin_unlock(&mutex->lock);

                logger("core: %d, task %d unlock mutex for task %d (priority: %d)\n",
                    get_current_cpu_id(), curr->task_id, task->task_id, task->priority);

                // 调度
                if (local_sched)
                    schedule();
                else
                {
                    gic_ipi_send_single(IPI_SCHED, target);
                    // logger("core: %d, send IPI to core: %d\n", get_current_cpu_id(), task->priority - 1);
                }
            }
        }
    }
}
