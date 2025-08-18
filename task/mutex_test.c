/**
 * @file mutex_test.c
 * @brief Mutex testing system calls for userspace debugging
 * 
 * 提供给用户态程序的 mutex 测试系统调用，用于验证内核 mutex 功能的正确性
 */

#include "task/mutex.h"
#include "task/task.h"
#include "io.h"
#include "lib/avatar_assert.h"
#include "thread.h"

// 专门用于测试的 mutex 和计数器
static mutex_t           test_mutex;
static volatile uint64_t mutex_test_counter = 6;

/**
 * @brief 初始化 mutex 测试模块
 * 应该在系统初始化时调用
 */
void
mutex_test_init(void)
{
    mutex_init(&test_mutex);
    mutex_test_counter = 6;
    logger("Mutex test module initialized\n");
}

/**
 * @brief Mutex 测试系统调用 - 加法测试
 * 
 * 执行大量的加减操作来测试 mutex 的互斥性
 * 在正确的 mutex 保护下，最终计数器值应该保持不变
 * 
 * @return 当前计数器值
 */
uint64_t
mutex_test_add(void)
{
    mutex_lock(&test_mutex);

    // 执行大量的加减操作，如果 mutex 工作正常，
    // 这些操作应该是原子的，不会被其他线程干扰
    for (int32_t i = 0; i < 10000; i++) {
        mutex_test_counter++;
        mutex_test_counter--;
        mutex_test_counter++;
        mutex_test_counter--;
        mutex_test_counter++;
        mutex_test_counter--;
        mutex_test_counter++;
        mutex_test_counter--;
    }

    mutex_unlock(&test_mutex);
    return mutex_test_counter;
}

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
mutex_test_minus(void)
{
    mutex_lock(&test_mutex);

    // 执行与 mutex_test_add 相同的操作
    for (int32_t i = 0; i < 10000; i++) {
        mutex_test_counter++;
        mutex_test_counter--;
        mutex_test_counter++;
        mutex_test_counter--;
        mutex_test_counter++;
        mutex_test_counter--;
        mutex_test_counter++;
        mutex_test_counter--;
    }

    mutex_unlock(&test_mutex);
    return mutex_test_counter;
}

/**
 * @brief Mutex 测试系统调用 - 打印当前状态
 * 
 * 打印当前计数器值和调用任务的信息
 * 用于调试和验证 mutex 测试的结果
 */
void
mutex_test_print(void)
{
    mutex_lock(&test_mutex);
    logger("mutex_test_counter = %llu, current task: %d\n",
           mutex_test_counter,
           curr_task_el1()->task_id);
    mutex_unlock(&test_mutex);
}

/**
 * @brief 重置 mutex 测试计数器
 * 
 * 将计数器重置为初始值，用于重新开始测试
 * 
 * @param initial_value 要设置的初始值，默认为 6
 */
void
mutex_test_reset(uint64_t initial_value)
{
    mutex_lock(&test_mutex);
    mutex_test_counter = initial_value;
    logger("Mutex test counter reset to %llu\n", initial_value);
    mutex_unlock(&test_mutex);
}

/**
 * @brief 获取当前 mutex 测试计数器值
 * 
 * @return 当前计数器值
 */
uint64_t
mutex_test_get_counter(void)
{
    uint64_t value;
    mutex_lock(&test_mutex);
    value = mutex_test_counter;
    mutex_unlock(&test_mutex);
    return value;
}
