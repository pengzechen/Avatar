#ifndef MUTEX_H
#define MUTEX_H

#include "task/task.h"
#include "list.h"

/**
 * 任务同步用的计数信号量
 */
typedef struct _mutex_t {
    tcb_t * owner;
    int locked_count;
    list_t wait_list;
}mutex_t;

void mutex_init (mutex_t * mutex);
void mutex_lock (mutex_t * mutex);
void mutex_unlock (mutex_t * mutex);


// uint64_t mutex_test_add();
// uint64_t mutex_test_minus();
// void mutex_test_print();
 
#endif //MUTEX_H