/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file fat32_fat.h
 * @brief Implementation of fat32_fat.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file fat32_fat.h
 * @brief FAT32文件分配表管理头文件
 * 
 * 本文件定义了FAT32文件分配表的管理功能，包括：
 * - FAT表的读写操作
 * - 簇链的创建、遍历和删除
 * - 空闲簇的分配和释放
 * - FAT表的缓存管理
 * 
 * FAT表是FAT32文件系统的核心数据结构，记录了每个簇的分配状态和链接关系。
 */

#ifndef FAT32_FAT_H
#define FAT32_FAT_H

#include "fat32_types.h"
#include "fat32_disk.h"

/* ============================================================================
 * FAT表操作函数
 * ============================================================================ */

/**
 * @brief 读取FAT表项
 * 
 * 从FAT表中读取指定簇的FAT表项值。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param cluster_num 簇号
 * @param fat_entry 返回的FAT表项值
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 计算FAT表项在磁盘中的位置
 * - 读取对应的扇区数据
 * - 提取FAT表项值
 */
fat32_error_t
fat32_fat_read_entry(fat32_disk_t          *disk,
                     const fat32_fs_info_t *fs_info,
                     uint32_t               cluster_num,
                     uint32_t              *fat_entry);

/**
 * @brief 写入FAT表项
 * 
 * 向FAT表中写入指定簇的FAT表项值。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param cluster_num 簇号
 * @param fat_entry FAT表项值
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 计算FAT表项在磁盘中的位置
 * - 读取对应的扇区数据
 * - 修改FAT表项值
 * - 写回所有FAT表副本
 */
fat32_error_t
fat32_fat_write_entry(fat32_disk_t          *disk,
                      const fat32_fs_info_t *fs_info,
                      uint32_t               cluster_num,
                      uint32_t               fat_entry);

/* ============================================================================
 * 簇分配和释放函数
 * ============================================================================ */

/**
 * @brief 分配一个空闲簇
 * 
 * 在FAT表中查找并分配一个空闲簇。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param cluster_num 返回分配的簇号
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 从next_free_cluster开始查找空闲簇
 * - 将找到的簇标记为簇链结束
 * - 更新FSInfo中的空闲簇信息
 */
fat32_error_t
fat32_fat_allocate_cluster(fat32_disk_t *disk, fat32_fs_info_t *fs_info, uint32_t *cluster_num);

/**
 * @brief 释放一个簇
 * 
 * 将指定簇标记为空闲状态。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param cluster_num 要释放的簇号
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 将簇的FAT表项设置为0（空闲）
 * - 更新FSInfo中的空闲簇信息
 * - 如果释放的簇号小于next_free_cluster，则更新next_free_cluster
 */
fat32_error_t
fat32_fat_free_cluster(fat32_disk_t *disk, fat32_fs_info_t *fs_info, uint32_t cluster_num);

/**
 * @brief 分配簇链
 * 
 * 分配指定数量的连续簇，形成簇链。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param cluster_count 需要分配的簇数量
 * @param first_cluster 返回第一个簇的簇号
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 分配指定数量的簇
 * - 将簇连接成链表
 * - 最后一个簇标记为簇链结束
 */
fat32_error_t
fat32_fat_allocate_cluster_chain(fat32_disk_t    *disk,
                                 fat32_fs_info_t *fs_info,
                                 uint32_t         cluster_count,
                                 uint32_t        *first_cluster);

/**
 * @brief 释放簇链
 * 
 * 释放从指定簇开始的整个簇链。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param first_cluster 簇链的第一个簇号
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 遍历簇链，释放每个簇
 * - 更新FSInfo中的空闲簇信息
 */
fat32_error_t
fat32_fat_free_cluster_chain(fat32_disk_t *disk, fat32_fs_info_t *fs_info, uint32_t first_cluster);

/* ============================================================================
 * 簇链操作函数
 * ============================================================================ */

/**
 * @brief 获取簇链的下一个簇
 * 
 * 读取指定簇的FAT表项，获取簇链中的下一个簇号。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param current_cluster 当前簇号
 * @param next_cluster 返回下一个簇号
 * @return fat32_error_t 错误码
 * 
 * 返回值说明：
 * - 如果是簇链结束，next_cluster为0
 * - 如果是坏簇，返回错误
 */
fat32_error_t
fat32_fat_get_next_cluster(fat32_disk_t          *disk,
                           const fat32_fs_info_t *fs_info,
                           uint32_t               current_cluster,
                           uint32_t              *next_cluster);

/**
 * @brief 扩展簇链
 * 
 * 在现有簇链的末尾添加新的簇。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param last_cluster 簇链的最后一个簇号
 * @param new_cluster 返回新分配的簇号
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 分配一个新簇
 * - 将last_cluster的FAT表项指向新簇
 * - 将新簇标记为簇链结束
 */
fat32_error_t
fat32_fat_extend_cluster_chain(fat32_disk_t    *disk,
                               fat32_fs_info_t *fs_info,
                               uint32_t         last_cluster,
                               uint32_t        *new_cluster);

/**
 * @brief 计算簇链长度
 * 
 * 遍历簇链，计算簇的总数量。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param first_cluster 簇链的第一个簇号
 * @param chain_length 返回簇链长度
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_fat_get_cluster_chain_length(fat32_disk_t          *disk,
                                   const fat32_fs_info_t *fs_info,
                                   uint32_t               first_cluster,
                                   uint32_t              *chain_length);

/**
 * @brief 获取簇链中的第N个簇
 * 
 * 在簇链中定位第N个簇的簇号。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param first_cluster 簇链的第一个簇号
 * @param cluster_index 簇索引（从0开始）
 * @param target_cluster 返回目标簇号
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_fat_get_cluster_at_index(fat32_disk_t          *disk,
                               const fat32_fs_info_t *fs_info,
                               uint32_t               first_cluster,
                               uint32_t               cluster_index,
                               uint32_t              *target_cluster);

/* ============================================================================
 * FAT表状态查询函数
 * ============================================================================ */

/**
 * @brief 检查簇是否为空闲
 * 
 * @param fat_entry FAT表项值
 * @return uint8_t 1表示空闲，0表示已分配
 */
static inline uint8_t
fat32_fat_is_free_cluster(uint32_t fat_entry)
{
    return (fat_entry == FAT32_FREE_CLUSTER);
}

/**
 * @brief 检查簇是否为簇链结束
 * 
 * @param fat_entry FAT表项值
 * @return uint8_t 1表示簇链结束，0表示不是
 */
static inline uint8_t
fat32_fat_is_end_of_chain(uint32_t fat_entry)
{
    return (fat_entry >= FAT32_EOC_MIN && fat_entry <= FAT32_EOC_MAX);
}

/**
 * @brief 检查簇是否为坏簇
 * 
 * @param fat_entry FAT表项值
 * @return uint8_t 1表示坏簇，0表示正常
 */
static inline uint8_t
fat32_fat_is_bad_cluster(uint32_t fat_entry)
{
    return (fat_entry == FAT32_BAD_CLUSTER);
}

/**
 * @brief 检查簇号是否有效
 * 
 * @param fs_info 文件系统信息
 * @param cluster_num 簇号
 * @return uint8_t 1表示有效，0表示无效
 */
static inline uint8_t
fat32_fat_is_valid_cluster(const fat32_fs_info_t *fs_info, uint32_t cluster_num)
{
    return (cluster_num >= 2 && cluster_num < fs_info->total_clusters + 2);
}

/**
 * @brief 计算FAT表项在扇区中的偏移
 * 
 * @param cluster_num 簇号
 * @return uint32_t 扇区内偏移（字节）
 */
static inline uint32_t
fat32_fat_get_entry_offset(uint32_t cluster_num)
{
    return (cluster_num * 4) % FAT32_SECTOR_SIZE;
}

/**
 * @brief 计算FAT表项所在的扇区
 * 
 * @param fs_info 文件系统信息
 * @param cluster_num 簇号
 * @return uint32_t 扇区号
 */
static inline uint32_t
fat32_fat_get_entry_sector(const fat32_fs_info_t *fs_info, uint32_t cluster_num)
{
    return fs_info->fat_start_sector + (cluster_num * 4) / FAT32_SECTOR_SIZE;
}

#endif  // FAT32_FAT_H
