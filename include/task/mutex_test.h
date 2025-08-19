/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file mutex_test.h
 * @brief Implementation of mutex_test.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file mutex_test.h
 * @brief Mutex testing system calls interface
 * 
 * 提供给用户态程序的 mutex 测试系统调用接口
 */

#ifndef __MUTEX_TEST_H__
#define __MUTEX_TEST_H__

#include "avatar_types.h"

/**
 * @brief 初始化 mutex 测试模块
 * 应该在系统初始化时调用
 */
void
mutex_test_init(void);

/**
 * @brief Mutex 测试系统调用 - 加法测试
 * 
 * 执行大量的加减操作来测试 mutex 的互斥性
 * 在正确的 mutex 保护下，最终计数器值应该保持不变
 * 
 * @return 当前计数器值
 */
uint64_t
mutex_test_add(void);

/**
 * @brief Mutex 测试系统调用 - 减法测试
 * 
 * 与 mutex_test_add 执行相同的操作，用于并发测试
 * 多个线程同时调用这两个函数时，如果 mutex 工作正常，
 * 计数器值应该保持稳定
 * 
 * @return 当前计数器值
 */
uint64_t
mutex_test_minus(void);

/**
 * @brief Mutex 测试系统调用 - 打印当前状态
 * 
 * 打印当前计数器值和调用任务的信息
 * 用于调试和验证 mutex 测试的结果
 */
void
mutex_test_print(void);

/**
 * @brief 重置 mutex 测试计数器
 * 
 * 将计数器重置为初始值，用于重新开始测试
 * 
 * @param initial_value 要设置的初始值，默认为 6
 */
void
mutex_test_reset(uint64_t initial_value);

/**
 * @brief 获取当前 mutex 测试计数器值
 * 
 * @return 当前计数器值
 */
uint64_t
mutex_test_get_counter(void);

#endif  // __MUTEX_TEST_H__
