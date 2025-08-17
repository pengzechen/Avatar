/**
 * @file kallocator.h
 * @brief Kernel Memory Allocator - 内核小块内存分配器
 * 
 * 提供内核小块内存的分配和释放功能，基于PMM进行页级别管理
 * 参考virtio_allocator的设计，支持任意大小的内存分配
 */

#ifndef __KALLOCATOR_H__
#define __KALLOCATOR_H__

#include "avatar_types.h"
#include "task/mutex.h"

/**
 * @brief 空闲块结构，用于管理已释放的内存
 */
typedef struct free_block {
    uint32_t size;              // 此空闲块的大小
    struct free_block *next;    // 链表中下一个空闲块
} free_block_t;

/**
 * @brief 页面信息结构，用于跟踪已分配的页面
 */
typedef struct page_info {
    void *page_addr;            // 页面地址
    uint32_t page_count;        // 连续页面数量
    uint32_t used_bytes;        // 页面中已使用的字节数
    uint32_t free_bytes;        // 页面中空闲的字节数
    bool is_large_alloc;        // 是否为大块分配（直接分配整页）
} page_info_t;

/**
 * @brief 内核内存分配器结构
 */
typedef struct {
    mutex_t mutex;              // 互斥锁
    free_block_t *free_list;    // 空闲块链表头

    // 页面管理
    page_info_t *pages;         // 已分配页面信息数组
    uint32_t page_count;        // 当前分配的页面数
    uint32_t page_capacity;     // 页面数组容量

    // 统计信息
    uint64_t total_size;        // 总分配大小（字节）
    uint64_t used_size;         // 已使用大小（字节）
    uint32_t small_alloc_count; // 小块分配计数
    uint32_t large_alloc_count; // 大块分配计数
} kallocator_t;

/**
 * @brief 初始化内核内存分配器
 * @return 0 成功，-1 失败
 */
int kallocator_init(void);

/**
 * @brief 分配指定大小的内存
 * @param size 要分配的字节数
 * @param alignment 对齐要求（字节），0表示使用默认对齐(8字节)
 * @return 分配的内存地址，失败返回NULL
 */
void *kalloc(uint32_t size, uint32_t alignment);

/**
 * @brief 分配指定大小的内存（使用默认对齐）
 * @param size 要分配的字节数
 * @return 分配的内存地址，失败返回NULL
 */
void *kalloc_simple(uint32_t size);

/**
 * @brief 释放内存
 * @param ptr 要释放的内存地址
 */
void kfree(void *ptr);

/**
 * @brief 分配对齐的内存
 * @param size 要分配的字节数
 * @param alignment 对齐要求（字节）
 * @return 分配的内存地址，失败返回NULL
 */
void *kalloc_aligned(uint32_t size, uint32_t alignment);

/**
 * @brief 获取已使用的内存大小
 * @return 已使用的字节数
 */
uint64_t kallocator_get_used_memory(void);

/**
 * @brief 获取空闲内存大小
 * @return 空闲的字节数
 */
uint64_t kallocator_get_free_memory(void);

/**
 * @brief 显示分配器信息
 */
void kallocator_info(void);

/**
 * @brief 测试内核分配器
 */
void kallocator_test(void);

/**
 * @brief 压力测试功能
 */
void kallocator_stress_test(void);

/**
 * @brief 检查分配器是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool kallocator_is_initialized(void);

/**
 * @brief 获取已分配页面数量
 * @return 当前分配的页面数量
 */
uint32_t kallocator_get_page_count(void);

/**
 * @brief 获取小块分配计数
 * @return 小块分配的数量
 */
uint32_t kallocator_get_small_alloc_count(void);

/**
 * @brief 获取大块分配计数
 * @return 大块分配的数量
 */
uint32_t kallocator_get_large_alloc_count(void);

/**
 * @brief 清理分配器并释放所有页面
 * @note 这会释放所有通过kalloc_pages分配的页面
 */
void kallocator_cleanup(void);

#endif // __KALLOCATOR_H__
