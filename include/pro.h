/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file pro.h
 * @brief Implementation of pro.h
 * @author Avatar Project Team
 * @date 2024
 */


#ifndef PRO_H
#define PRO_H

#include "avatar_types.h"
#include "os_cfg.h"
#include "task/task.h"
#include "lib/list.h"

#define PRO_MAX_NAME_LEN 64

typedef struct _tcb_t tcb_t;

typedef struct _process_t
{
    uint32_t process_id;                      // 进程id
    char     process_name[PRO_MAX_NAME_LEN];  // 进程名

    void *el1_stack;  // el1 的栈, TODO: 应该是一个task一个
    void *el0_stack;  // el0 的栈
    void *pg_base;    // 进程页表基地址
    void *heap_start;
    void *heap_end;

    uint64_t entry;
    list_t   threads;      // 任务列表， 一个进程可以有很多线程
    tcb_t   *main_thread;  // 主线程

    struct _process_t *parent;
} process_t;

typedef struct _process_manager_t
{
    list_t processes;  // 进程列表

} process_manager_t;

process_t *
alloc_process(char *name);
void
process_init(process_t *pro, void *elf_addr, uint32_t affinity);
void
run_process(process_t *pro);

int32_t
pro_execve(char *name, char **__argv, char **__envp);
int32_t
pro_fork(void);

#endif  // PRO_H