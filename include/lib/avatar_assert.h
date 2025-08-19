/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file avatar_assert.h
 * @brief Implementation of avatar_assert.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file avatar_assert.h
 * @brief Avatar OS 断言系统
 * 
 * 提供统一的断言机制，支持调试和发布版本的不同行为
 */

#ifndef __AVATAR_ASSERT_H__
#define __AVATAR_ASSERT_H__

#include "avatar_types.h"
#include "io.h"

/* ==================== 断言配置 ==================== */

/**
 * @brief 断言失败处理函数类型
 * @param condition 失败的条件字符串
 * @param function 函数名
 * @param file 文件名
 * @param line 行号
 */
typedef void (*assert_handler_t)(const char *condition,
                                 const char *function,
                                 const char *file,
                                 int         line);

/* ==================== 断言实现 ==================== */

/**
 * @brief 默认断言失败处理函数
 * @param condition 失败的条件字符串
 * @param function 函数名
 * @param file 文件名
 * @param line 行号
 */
static inline void
avatar_assert_fail_default(const char *condition, const char *function, const char *file, int line)
{
    logger("ASSERTION FAILED: %s\n", condition);
    logger("  Function: %s\n", function);
    logger("  File: %s:%d\n", file, line);

    // 在内核中，断言失败应该停止系统
    while (1) {
        __asm__ volatile("wfi");  // 等待中断，节省电力
    }
}

/**
 * @brief 轻量级断言失败处理（用于性能敏感的代码）
 * @param condition 失败的条件字符串
 * @param function 函数名
 * @param file 文件名
 * @param line 行号
 */
static inline void
avatar_assert_fail_lite(const char *condition, const char *function, const char *file, int line)
{
    logger("ASSERT: %s at %s:%d\n", condition, file, line);
    while (1) {
        __asm__ volatile("wfi");
    }
}

/* ==================== 断言宏定义 ==================== */

#ifdef NDEBUG
    /* 发布版本：禁用断言 */
    #define avatar_assert(condition)          ((void) 0)
    #define avatar_assert_msg(condition, msg) ((void) 0)
    #define avatar_assert_lite(condition)     ((void) 0)
#else
    /* 调试版本：启用断言 */

    /**
     * @brief 标准断言宏
     * @param condition 要检查的条件
     */
    #define avatar_assert(condition)                                                               \
        do {                                                                                       \
            if (!(condition)) {                                                                    \
                avatar_assert_fail_default(#condition, __func__, __FILE__, __LINE__);              \
            }                                                                                      \
        } while (0)

    /**
     * @brief 带消息的断言宏
     * @param condition 要检查的条件
     * @param msg 额外的错误消息
     */
    #define avatar_assert_msg(condition, msg)                                                      \
        do {                                                                                       \
            if (!(condition)) {                                                                    \
                logger("ASSERTION FAILED: %s\n", #condition);                                      \
                logger("  Message: %s\n", msg);                                                    \
                logger("  Function: %s\n", __func__);                                              \
                logger("  File: %s:%d\n", __FILE__, __LINE__);                                     \
                while (1) {                                                                        \
                    __asm__ volatile("wfi");                                                       \
                }                                                                                  \
            }                                                                                      \
        } while (0)

#endif

/* ==================== 静态断言 ==================== */

/**
 * @brief 编译时断言宏
 * @param condition 要检查的编译时常量条件
 * @param msg 错误消息
 */
#define avatar_static_assert(condition, msg) _Static_assert(condition, msg)

/* ==================== 调试辅助宏 ==================== */

/**
 * @brief 不应该到达的代码路径
 */
#define avatar_unreachable() avatar_assert_msg(0, "This code path should never be reached")

/**
 * @brief 未实现的功能标记
 */
#define avatar_unimplemented() avatar_assert_msg(0, "This functionality is not yet implemented")

#endif  // __AVATAR_ASSERT_H__
