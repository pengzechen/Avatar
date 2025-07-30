
#ifndef PRO_H
#define PRO_H

#include "aj_types.h"
#include "os_cfg.h"
#include "task/task.h"
#include "lib/list.h"

#define PRO_MAX_NAME_LEN 64

typedef struct _tcb_t tcb_t;

typedef struct _process_t
{
    uint32_t process_id;                 // 进程id
    char process_name[PRO_MAX_NAME_LEN]; // 进程名

    void *el1_stack; // el1 的栈, TODO: 应该是一个task一个
    void *el0_stack; // el0 的栈
    void *pg_base;   // 进程页表基地址
    void *heap_start;
    void *heap_end;

    uint64_t entry;
    list_t threads;     // 任务列表， 一个进程可以有很多线程
    tcb_t *main_thread; // 主线程

    struct _process_t *parent;
} process_t;

typedef struct _process_manager_t
{
    list_t processes; // 进程列表

} process_manager_t;

process_t *alloc_process(char *name);
void process_init(process_t *pro, void *elf_addr, uint32_t priority);
void run_process(process_t *pro);

int pro_execve(char *name, char **__argv, char **__envp);
int pro_fork(void);

#endif // PRO_H