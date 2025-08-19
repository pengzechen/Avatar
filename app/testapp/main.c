/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file main.c
 * @brief Implementation of main.c
 * @author Avatar Project Team
 * @date 2024
 */


#include "syscall.h"

// void test_shell() {
//     char c;
//     while (1) {
//         // 获取用户输入字符
//         c = getc();

//         // 如果输入回车键，换行
//         if (c == '\r') {
//             putc('\r');
//             putc('\n');
//             continue;
//         }

//         // 输出输入字符
//         putc(c);

//         // 如果输入是 'a'，fork 一个子进程执行 execve("add", ...)
//         if (c == 'a') {
//             int pid = fork();
//             if (pid == 0) {  // 子进程执行
//                 execve("add", (void*)0x40147000);
//             }
//             // 父进程继续循环等待下一次输入
//             // sleep(1000000);
//         }
//         // 如果输入是 'b'，fork 一个子进程执行 execve("sub", ...)
//         else if (c == 'b') {
//             int pid = fork();
//             if (pid == 0) {  // 子进程执行
//                 execve("sub", (void*)0x40147000);
//             }
//             // 父进程继续循环等待下一次输入
//             // sleep(1000000);
//         }
//     }
// }

char *argv[] = {"argv1", "argv2", "argv3", NULL};

char *envp[] = {"test envp1", "test envp2", "test envp3", NULL};

int
_start()
{
    int pid = fork();
    if (pid == 0) {  // 子进程执行
        execve("add", argv, envp);
    } else {
        execve("sub", argv, envp);
    }
    return 0;
}