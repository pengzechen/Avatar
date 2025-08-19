/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file syscall_num.h
 * @brief Implementation of syscall_num.h
 * @author Avatar Project Team
 * @date 2024
 */



#ifndef SYSCALL_NUM_H
#define SYSCALL_NUM_H

#define NR_SYSCALL 256

#define SYS_putc   0
#define SYS_getc   1
#define SYS_sleep  2
#define SYS_execve 3
#define SYS_fork   4

#define SYS_mutex_test_print 252
#define SYS_mutex_add        253
#define SYS_mutex_minus      254
#define SYS_debug            255

#endif  // SYSCALL_NUM_H