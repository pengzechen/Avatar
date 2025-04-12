

#include "task/mutex.h"
#include "task/task.h"
#include "io.h"
#include "thread.h"
#include "gic.h"

// 初始化互斥锁
void mutex_init (mutex_t * mutex) {
    // 将互斥锁的锁定计数器初始化为0
    mutex->locked_count = 0;
    // 将互斥锁的所有者初始化为空指针
    mutex->owner = (tcb_t *)0;
    // 初始化互斥锁的等待列表
    list_init(&mutex->wait_list);
}


// 定义一个函数，用于锁定互斥锁
void mutex_lock (mutex_t * mutex) {

    disable_interrupts();
    // 获取当前线程控制块
    tcb_t * curr = (tcb_t*)(void*)read_tpidr_el0();
    // 如果互斥锁没有被锁定
    if (mutex->locked_count == 0) {
        // 将互斥锁的锁定计数设置为1
        mutex->locked_count = 1;
        // 将互斥锁的所有者设置为当前线程控制块
        mutex->owner = curr;
    // 如果互斥锁已经被当前线程锁定
    } else if (mutex->owner == curr) {
        // 将互斥锁的锁定计数加1
        mutex->locked_count++;
    // 如果互斥锁已经被其他线程锁定
    } else {
        // 获取当前线程控制块
        tcb_t * curr = (tcb_t*)(void*)read_tpidr_el0();
        // 将当前线程控制块设置为阻塞状态
        task_set_block(curr);
        // 将当前线程控制块插入互斥锁的等待列表中
        list_insert_last(&mutex->wait_list, &curr->wait_node);

        printf("mutex lock by other task, current(task %d) yield!\n", curr->id);
        // 调度其他线程
        schedule();
    }
    enable_interrupts();
}


// 解锁互斥锁
void mutex_unlock (mutex_t * mutex) {

    disable_interrupts();
    // 获取当前线程控制块
    tcb_t * curr = (tcb_t*)(void*)read_tpidr_el0();
    // 如果当前线程是互斥锁的拥有者
    if (mutex->owner == curr) {
        // 互斥锁的锁定计数减一
        if (--mutex->locked_count == 0) {
            // 将互斥锁的拥有者置为空
            mutex->owner = (tcb_t *)0;

            // 如果等待队列中有线程
            if (list_count(&mutex->wait_list)) {
                // 移除等待队列中的第一个线程
                list_node_t * task_node = list_delete_first(&mutex->wait_list);
                // 获取该线程的控制块
                tcb_t * task = list_node_parent(task_node, tcb_t, wait_node);
                // 将该线程设为就绪状态
                task_set_ready(task);

                // 互斥锁的锁定计数置为1
                mutex->locked_count = 1;
                // 将互斥锁的拥有者置为该线程
                mutex->owner = task;

                printf("mutex unlock, task %d\n", task->id);
                // 调度
                schedule();
            }
        }
    }
    enable_interrupts();
}





