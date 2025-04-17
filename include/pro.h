
#ifndef PRO_H
#define PRO_H

#include "aj_types.h"
#include "os_cfg.h"
#include "task/task.h"
#include "list.h"

#define PRO_MAX_NAME_LEN        64

typedef struct _process_t {
    uint32_t process_id;   // 进程id
    char process_name[PRO_MAX_NAME_LEN]; // 进程名
    list_t threads;        // 任务列表

    void* el1_stack;    // el1 的栈
    void* el0_stack;
    void* pg_base;      // 进程页表基地址
    void* heap_start;
    void* heap_end;

    uint64_t entry;
} process_t;



typedef struct _process_manager_t {
    list_t processes;     // 进程列表

} process_manager_t;

process_t *alloc_process(char *name);
void process_init(process_t *pro, void *elf_addr, uint32_t priority);
void run_process(process_t *pro);

#endif // PRO_H