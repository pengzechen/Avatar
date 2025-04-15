
#ifndef PRO_H
#define PRO_H

#include "aj_types.h"
#include "os_cfg.h"
#include "task.h"
#include "list.h"

#define PRO_MAX_NAME_LEN        64

typedef struct _process_t {
    uint32_t process_id;   // 进程id
    char process_name[PRO_MAX_NAME_LEN]; // 进程名
    list_t threads;        // 任务列表

    void* pg_base;      // 进程页表基地址
    void* el1_stack;    // el1 的栈
    
    void* heap_start;
    void* heap_end;

    uint64_t entry;
} process_t;



typedef struct _process_manager_t {
    list_t processes;     // 进程列表

} process_manager_t;

process_t *create_process(char *name);

#endif // PRO_H