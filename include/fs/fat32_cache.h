/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file fat32_cache.h
 * @brief Implementation of fat32_cache.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file fat32_cache.h
 * @brief FAT32缓存管理头文件
 * 
 * 本文件定义了FAT32文件系统的缓存管理功能。
 * 为了简化实现，当前版本提供基本的缓存接口，但实际缓存功能有限。
 * 主要用于保持代码结构的完整性，便于将来扩展。
 * 
 * 简化说明：
 * - 当前实现主要是接口占位，实际缓存逻辑较简单
 * - 重点在于提供统一的缓存访问接口
 * - 可以在此基础上扩展更复杂的缓存策略
 */

#ifndef FAT32_CACHE_H
#define FAT32_CACHE_H

#include "fat32_types.h"
#include "fat32_disk.h"

/* ============================================================================
 * 缓存配置常量
 * ============================================================================ */

#define FAT32_CACHE_SIZE       16    // 缓存块数量
#define FAT32_CACHE_BLOCK_SIZE 4096  // 缓存块大小（一个簇）

/* 缓存块状态 */
#define FAT32_CACHE_CLEAN 0  // 干净（与磁盘同步）
#define FAT32_CACHE_DIRTY 1  // 脏（需要写回磁盘）
#define FAT32_CACHE_FREE  2  // 空闲

/* ============================================================================
 * 缓存数据结构
 * ============================================================================ */

/**
 * @brief 缓存块结构
 */
typedef struct
{
    uint32_t cluster_num;  // 缓存的簇号
    uint8_t *data;         // 缓存数据
    uint8_t  status;       // 缓存状态
    uint32_t access_time;  // 最后访问时间（简化的LRU）
    uint8_t  in_use;       // 是否在使用中
} fat32_cache_block_t;

/**
 * @brief 缓存管理器结构
 */
typedef struct
{
    fat32_cache_block_t blocks[FAT32_CACHE_SIZE];  // 缓存块数组
    uint32_t            access_counter;            // 访问计数器
    uint8_t             initialized;               // 初始化标志
} fat32_cache_manager_t;

/* ============================================================================
 * 缓存管理函数
 * ============================================================================ */

/**
 * @brief 初始化缓存管理器
 * 
 * 分配缓存内存并初始化缓存管理结构。
 * 
 * @param cache_mgr 缓存管理器指针
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cache_init(fat32_cache_manager_t *cache_mgr);

/**
 * @brief 清理缓存管理器
 * 
 * 刷新所有脏缓存块并释放内存。
 * 
 * @param cache_mgr 缓存管理器指针
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cache_cleanup(fat32_cache_manager_t *cache_mgr,
                    fat32_disk_t          *disk,
                    const fat32_fs_info_t *fs_info);

/**
 * @brief 读取簇数据（通过缓存）
 * 
 * 从缓存中读取簇数据，如果缓存中没有则从磁盘读取。
 * 
 * @param cache_mgr 缓存管理器指针
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param cluster_num 簇号
 * @param buffer 数据缓冲区
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cache_read_cluster(fat32_cache_manager_t *cache_mgr,
                         fat32_disk_t          *disk,
                         const fat32_fs_info_t *fs_info,
                         uint32_t               cluster_num,
                         void                  *buffer);

/**
 * @brief 写入簇数据（通过缓存）
 * 
 * 将数据写入缓存，标记为脏块。
 * 
 * @param cache_mgr 缓存管理器指针
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param cluster_num 簇号
 * @param buffer 数据缓冲区
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cache_write_cluster(fat32_cache_manager_t *cache_mgr,
                          fat32_disk_t          *disk,
                          const fat32_fs_info_t *fs_info,
                          uint32_t               cluster_num,
                          const void            *buffer);

/**
 * @brief 刷新缓存
 * 
 * 将所有脏缓存块写回磁盘。
 * 
 * @param cache_mgr 缓存管理器指针
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cache_flush(fat32_cache_manager_t *cache_mgr,
                  fat32_disk_t          *disk,
                  const fat32_fs_info_t *fs_info);

/**
 * @brief 刷新指定簇的缓存
 * 
 * 将指定簇的缓存块写回磁盘（如果是脏块）。
 * 
 * @param cache_mgr 缓存管理器指针
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param cluster_num 簇号
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cache_flush_cluster(fat32_cache_manager_t *cache_mgr,
                          fat32_disk_t          *disk,
                          const fat32_fs_info_t *fs_info,
                          uint32_t               cluster_num);

/**
 * @brief 使缓存块无效
 * 
 * 将指定簇的缓存块标记为无效，强制下次从磁盘重新读取。
 * 
 * @param cache_mgr 缓存管理器指针
 * @param cluster_num 簇号
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cache_invalidate_cluster(fat32_cache_manager_t *cache_mgr, uint32_t cluster_num);

/**
 * @brief 获取缓存统计信息
 * 
 * 获取缓存的命中率、脏块数量等统计信息。
 * 
 * @param cache_mgr 缓存管理器指针
 * @param total_blocks 返回总缓存块数
 * @param used_blocks 返回已使用的缓存块数
 * @param dirty_blocks 返回脏缓存块数
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cache_get_stats(const fat32_cache_manager_t *cache_mgr,
                      uint32_t                    *total_blocks,
                      uint32_t                    *used_blocks,
                      uint32_t                    *dirty_blocks);

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */

/**
 * @brief 检查缓存管理器是否已初始化
 * 
 * @param cache_mgr 缓存管理器指针
 * @return uint8_t 1表示已初始化，0表示未初始化
 */
static inline uint8_t
fat32_cache_is_initialized(const fat32_cache_manager_t *cache_mgr)
{
    return (cache_mgr != NULL && cache_mgr->initialized);
}

/**
 * @brief 检查缓存块是否为脏块
 * 
 * @param block 缓存块指针
 * @return uint8_t 1表示脏块，0表示干净块
 */
static inline uint8_t
fat32_cache_is_dirty_block(const fat32_cache_block_t *block)
{
    return (block->status == FAT32_CACHE_DIRTY);
}

/**
 * @brief 检查缓存块是否空闲
 * 
 * @param block 缓存块指针
 * @return uint8_t 1表示空闲，0表示已使用
 */
static inline uint8_t
fat32_cache_is_free_block(const fat32_cache_block_t *block)
{
    return (block->status == FAT32_CACHE_FREE);
}

/**
 * @brief 标记缓存块为脏块
 * 
 * @param block 缓存块指针
 */
static inline void
fat32_cache_mark_dirty(fat32_cache_block_t *block)
{
    block->status = FAT32_CACHE_DIRTY;
}

/**
 * @brief 标记缓存块为干净块
 * 
 * @param block 缓存块指针
 */
static inline void
fat32_cache_mark_clean(fat32_cache_block_t *block)
{
    block->status = FAT32_CACHE_CLEAN;
}

/* ============================================================================
 * 全局缓存管理器访问函数
 * ============================================================================ */

/**
 * @brief 获取全局缓存管理器实例
 * 
 * @return fat32_cache_manager_t* 缓存管理器指针
 */
fat32_cache_manager_t *
fat32_get_cache_manager(void);

/**
 * @brief 初始化全局缓存管理器
 * 
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_init_global_cache(void);

/**
 * @brief 清理全局缓存管理器
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cleanup_global_cache(fat32_disk_t *disk, const fat32_fs_info_t *fs_info);

#endif  // FAT32_CACHE_H
