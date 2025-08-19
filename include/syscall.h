/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file syscall.h
 * @brief Implementation of syscall.h
 * @author Avatar Project Team
 * @date 2024
 */


#ifndef SYSCALL_H
#define SYSCALL_H
#include "avatar_types.h"

char
getc();
void
putc(char c);
void
sleep(uint64_t ms);
int32_t
execve(char *name, char **__argv, char **__envp);
int32_t
fork();

uint64_t
mutex_test_add();
uint64_t
mutex_test_minus();
void
mutex_test_print();

#endif  // SYSCALL_H