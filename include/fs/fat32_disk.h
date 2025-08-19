/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file fat32_disk.h
 * @brief Implementation of fat32_disk.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file fat32_disk.h
 * @brief FAT32磁盘操作抽象层头文件
 * 
 * 本文件定义了FAT32文件系统的底层磁盘访问接口。
 * 为了简化实现，我们使用内存模拟磁盘，但保持了标准的磁盘访问接口，
 * 便于将来扩展到真实的磁盘设备。
 * 
 * 主要功能：
 * - 扇区级别的读写操作
 * - 磁盘初始化和格式化
 * - 错误处理和状态管理
 */

#ifndef FAT32_DISK_H
#define FAT32_DISK_H

#include "fat32_types.h"
#include "avatar_types.h"

/* ============================================================================
 * 磁盘配置常量
 * ============================================================================ */

#define FAT32_DISK_SIZE           (64 * 1024 * 1024)  // 64MB虚拟磁盘
#define FAT32_TOTAL_SECTORS       (FAT32_DISK_SIZE / FAT32_SECTOR_SIZE)
#define FAT32_SECTORS_PER_CLUSTER 8   // 每簇8个扇区（4KB）
#define FAT32_RESERVED_SECTORS    32  // 保留扇区数
#define FAT32_NUM_FATS            2   // FAT表数量
#define FAT32_ROOT_CLUSTER        2   // 根目录起始簇号

/* 计算得出的磁盘布局参数 */
#define FAT32_CLUSTERS_COUNT                                                                       \
    ((FAT32_TOTAL_SECTORS - FAT32_RESERVED_SECTORS) /                                              \
     (FAT32_SECTORS_PER_CLUSTER + (FAT32_NUM_FATS * 4 / FAT32_SECTOR_SIZE)))
#define FAT32_FAT_SIZE_SECTORS                                                                     \
    ((FAT32_CLUSTERS_COUNT * 4 + FAT32_SECTOR_SIZE - 1) / FAT32_SECTOR_SIZE)
#define FAT32_DATA_START_SECTOR (FAT32_RESERVED_SECTORS + FAT32_NUM_FATS * FAT32_FAT_SIZE_SECTORS)

/* ============================================================================
 * 磁盘状态结构
 * ============================================================================ */

/**
 * @brief 虚拟磁盘状态结构
 * 
 * 维护虚拟磁盘的状态信息和统计数据
 */
typedef struct
{
    uint8_t *disk_data;      // 磁盘数据指针
    uint32_t disk_size;      // 磁盘大小（字节）
    uint32_t total_sectors;  // 总扇区数
    uint8_t  initialized;    // 初始化标志
    uint8_t  formatted;      // 格式化标志

    /* 统计信息 */
    uint32_t read_count;   // 读操作计数
    uint32_t write_count;  // 写操作计数
    uint32_t error_count;  // 错误计数
} fat32_disk_t;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
 * @brief 初始化虚拟磁盘
 * 
 * 分配内存空间作为虚拟磁盘，并初始化磁盘状态结构。
 * 
 * @param disk 磁盘状态结构指针
 * @return fat32_error_t 错误码
 * 
 * 简化说明：使用内存模拟磁盘，实际项目中可替换为真实磁盘驱动
 */
fat32_error_t
fat32_disk_init(fat32_disk_t *disk);

/**
 * @brief 清理磁盘资源
 * 
 * 释放磁盘占用的内存资源。
 * 
 * @param disk 磁盘状态结构指针
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_disk_cleanup(fat32_disk_t *disk);

/**
 * @brief 读取磁盘扇区
 * 
 * 从指定扇区读取数据到缓冲区。
 * 
 * @param disk 磁盘状态结构指针
 * @param sector_num 扇区号（从0开始）
 * @param sector_count 要读取的扇区数量
 * @param buffer 数据缓冲区
 * @return fat32_error_t 错误码
 * 
 * 注意：缓冲区大小必须至少为 sector_count * FAT32_SECTOR_SIZE 字节
 */
fat32_error_t
fat32_disk_read_sectors(fat32_disk_t *disk,
                        uint32_t      sector_num,
                        uint32_t      sector_count,
                        void         *buffer);

/**
 * @brief 写入磁盘扇区
 * 
 * 将缓冲区数据写入到指定扇区。
 * 
 * @param disk 磁盘状态结构指针
 * @param sector_num 扇区号（从0开始）
 * @param sector_count 要写入的扇区数量
 * @param buffer 数据缓冲区
 * @return fat32_error_t 错误码
 * 
 * 注意：缓冲区大小必须至少为 sector_count * FAT32_SECTOR_SIZE 字节
 */
fat32_error_t
fat32_disk_write_sectors(fat32_disk_t *disk,
                         uint32_t      sector_num,
                         uint32_t      sector_count,
                         const void   *buffer);

/**
 * @brief 格式化磁盘为FAT32
 * 
 * 在虚拟磁盘上创建FAT32文件系统结构，包括：
 * - 引导扇区
 * - FSInfo结构
 * - FAT表
 * - 根目录
 * 
 * @param disk 磁盘状态结构指针
 * @param volume_label 卷标（最多11个字符）
 * @return fat32_error_t 错误码
 * 
 * 简化说明：创建最基本的FAT32结构，不包含复杂的优化
 */
fat32_error_t
fat32_disk_format(fat32_disk_t *disk, const char *volume_label);

/**
 * @brief 获取磁盘信息
 * 
 * 获取磁盘的基本信息和统计数据。
 * 
 * @param disk 磁盘状态结构指针
 * @param total_sectors 返回总扇区数
 * @param free_sectors 返回空闲扇区数（简化实现中可能不准确）
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_disk_get_info(fat32_disk_t *disk, uint32_t *total_sectors, uint32_t *free_sectors);

/**
 * @brief 同步磁盘数据
 * 
 * 确保所有缓存的数据都写入到磁盘。
 * 在内存模拟的情况下，这个函数主要用于保持接口一致性。
 * 
 * @param disk 磁盘状态结构指针
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_disk_sync(fat32_disk_t *disk);

/**
 * @brief 检查扇区是否有效
 * 
 * 验证扇区号是否在有效范围内。
 * 
 * @param disk 磁盘状态结构指针
 * @param sector_num 扇区号
 * @return uint8_t 1表示有效，0表示无效
 */
uint8_t
fat32_disk_is_valid_sector(fat32_disk_t *disk, uint32_t sector_num);

/**
 * @brief 获取磁盘统计信息
 * 
 * 获取磁盘的读写统计信息，用于性能分析和调试。
 * 
 * @param disk 磁盘状态结构指针
 * @param read_count 返回读操作次数
 * @param write_count 返回写操作次数
 * @param error_count 返回错误次数
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_disk_get_stats(fat32_disk_t *disk,
                     uint32_t     *read_count,
                     uint32_t     *write_count,
                     uint32_t     *error_count);

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */

/**
 * @brief 计算扇区在磁盘中的字节偏移
 * 
 * @param sector_num 扇区号
 * @return uint32_t 字节偏移
 */
static inline uint32_t
fat32_disk_sector_to_offset(uint32_t sector_num)
{
    return sector_num * FAT32_SECTOR_SIZE;
}

/**
 * @brief 计算字节偏移对应的扇区号
 * 
 * @param offset 字节偏移
 * @return uint32_t 扇区号
 */
static inline uint32_t
fat32_disk_offset_to_sector(uint32_t offset)
{
    return offset / FAT32_SECTOR_SIZE;
}

/* ============================================================================
 * 全局磁盘实例访问函数
 * ============================================================================ */

/**
 * @brief 获取全局磁盘实例
 *
 * @return fat32_disk_t* 全局磁盘实例指针
 */
fat32_disk_t *
fat32_get_disk(void);

/**
 * @brief 检查是否使用VirtIO块设备
 *
 * @return uint8_t 1表示使用VirtIO，0表示使用内存模拟
 */
uint8_t
fat32_disk_is_using_virtio(void);

/**
 * @brief 获取底层设备信息
 *
 * @param disk 磁盘状态结构指针
 * @param device_capacity 返回设备容量（扇区数）
 * @param device_block_size 返回设备块大小（字节）
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_disk_get_device_info(fat32_disk_t *disk,
                           uint64_t     *device_capacity,
                           uint32_t     *device_block_size);

/**
 * @brief 打印磁盘信息
 *
 * @param disk 磁盘状态结构指针
 */
void
fat32_disk_print_info(fat32_disk_t *disk);

/**
 * @brief 测试磁盘读写功能
 *
 * @param disk 磁盘状态结构指针
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_disk_test_rw(fat32_disk_t *disk);

#endif  // FAT32_DISK_H
