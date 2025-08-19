/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file pmm.h
 * @brief Implementation of pmm.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file pmm.h
 * @brief Physical Memory Manager - 物理内存管理器
 * 
 * 提供物理内存的分配和释放功能，独立于虚拟内存管理
 */

#ifndef __PMM_H__
#define __PMM_H__

#include "avatar_types.h"
#include "task/mutex.h"
#include "mem/bitmap.h"

/**
 * @brief 物理内存分配器结构
 */
typedef struct
{
    mutex_t  mutex;        // 互斥锁
    bitmap_t bitmap;       // 页面分配位图
    uint64_t start_addr;   // 物理内存起始地址
    uint64_t total_size;   // 总内存大小
    uint64_t page_size;    // 页面大小
    uint64_t total_pages;  // 总页面数
    uint64_t free_pages;   // 空闲页面数
} pmm_t;

/**
 * @brief 初始化物理内存管理器
 * @param pmm 物理内存管理器实例
 * @param start_addr 物理内存起始地址
 * @param size 内存大小
 * @param bitmap_buffer 位图缓冲区
 * @param bitmap_size 位图大小
 */
void
pmm_init(pmm_t   *pmm,
         uint64_t start_addr,
         uint64_t size,
         uint8_t *bitmap_buffer,
         size_t   bitmap_size);

/**
 * @brief 分配连续的物理页面
 * @param pmm 物理内存管理器
 * @param page_count 需要分配的页面数
 * @return 分配的物理地址，失败返回0
 */
uint64_t
pmm_alloc_pages(pmm_t *pmm, uint32_t page_count);

/**
 * @brief 分配连续的物理页面（使用first-fit策略）
 * @param pmm 物理内存管理器
 * @param page_count 需要分配的页面数
 * @return 分配的物理地址，失败返回0
 */
uint64_t
pmm_alloc_pages_fs(pmm_t *pmm, uint32_t page_count);

/**
 * @brief 释放物理页面
 * @param pmm 物理内存管理器
 * @param paddr 物理地址
 * @param page_count 页面数
 */
void
pmm_free_pages(pmm_t *pmm, uint64_t paddr, uint32_t page_count);

/**
 * @brief 标记内存区域为已分配（用于标记内核等保留区域）
 * @param pmm 物理内存管理器
 * @param start_addr 起始地址
 * @param end_addr 结束地址
 */
void
pmm_mark_allocated(pmm_t *pmm, uint64_t start_addr, uint64_t end_addr);

/**
 * @brief 获取空闲页面数
 * @param pmm 物理内存管理器
 * @return 空闲页面数
 */
uint64_t
pmm_get_free_pages(pmm_t *pmm);

/**
 * @brief 获取总页面数
 * @param pmm 物理内存管理器
 * @return 总页面数
 */
uint64_t
pmm_get_total_pages(pmm_t *pmm);

/**
 * @brief 标记内核内存区域为已分配
 * @param pmm 物理内存管理器
 */
void
pmm_mark_kernel_allocated(pmm_t *pmm);

#endif  // __PMM_H__
