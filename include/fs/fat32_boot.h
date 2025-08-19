/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file fat32_boot.h
 * @brief Implementation of fat32_boot.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file fat32_boot.h
 * @brief FAT32引导扇区管理头文件
 * 
 * 本文件定义了FAT32引导扇区的读取、解析和验证功能。
 * 引导扇区包含了文件系统的基本参数，是挂载文件系统的关键信息。
 * 
 * 主要功能：
 * - 读取和解析引导扇区
 * - 验证文件系统有效性
 * - 计算文件系统布局参数
 * - 读取FSInfo结构
 */

#ifndef FAT32_BOOT_H
#define FAT32_BOOT_H

#include "fat32_types.h"
#include "fat32_disk.h"

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
 * @brief 读取并解析引导扇区
 * 
 * 从磁盘读取引导扇区，解析其中的文件系统参数，并填充文件系统信息结构。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息结构指针
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 读取扇区0的引导扇区数据
 * - 验证引导扇区签名和基本参数
 * - 计算文件系统布局参数
 * - 初始化文件系统信息结构
 */
fat32_error_t
fat32_boot_read_boot_sector(fat32_disk_t *disk, fat32_fs_info_t *fs_info);

/**
 * @brief 验证引导扇区有效性
 * 
 * 检查引导扇区的各项参数是否符合FAT32规范。
 * 
 * @param boot_sector 引导扇区结构指针
 * @return fat32_error_t 错误码
 * 
 * 验证项目：
 * - 引导扇区签名（0xAA55）
 * - 扇区大小（必须为512字节）
 * - 每簇扇区数（必须为2的幂）
 * - FAT表数量（通常为2）
 * - 文件系统类型标识
 */
fat32_error_t
fat32_boot_validate_boot_sector(const fat32_boot_sector_t *boot_sector);

/**
 * @brief 计算文件系统布局参数
 * 
 * 根据引导扇区的参数计算文件系统的各个区域位置。
 * 
 * @param boot_sector 引导扇区结构指针
 * @param fs_info 文件系统信息结构指针
 * @return fat32_error_t 错误码
 * 
 * 计算内容：
 * - FAT表起始扇区
 * - 数据区起始扇区
 * - 总簇数
 * - 每簇字节数
 */
fat32_error_t
fat32_boot_calculate_layout(const fat32_boot_sector_t *boot_sector, fat32_fs_info_t *fs_info);

/**
 * @brief 读取FSInfo结构
 * 
 * 读取并解析FSInfo结构，获取文件系统状态信息。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息结构指针
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 读取FSInfo扇区数据
 * - 验证FSInfo签名
 * - 提取空闲簇信息
 * - 更新文件系统状态
 */
fat32_error_t
fat32_boot_read_fsinfo(fat32_disk_t *disk, fat32_fs_info_t *fs_info);

/**
 * @brief 写入FSInfo结构
 * 
 * 将文件系统状态信息写入FSInfo结构。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息结构指针
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 更新FSInfo中的空闲簇信息
 * - 设置下一个空闲簇提示
 * - 写入磁盘
 */
fat32_error_t
fat32_boot_write_fsinfo(fat32_disk_t *disk, const fat32_fs_info_t *fs_info);

/**
 * @brief 验证FSInfo结构
 * 
 * 检查FSInfo结构的签名和数据有效性。
 * 
 * @param fsinfo FSInfo结构指针
 * @return fat32_error_t 错误码
 * 
 * 验证项目：
 * - 前导签名（0x41615252）
 * - 结构签名（0x61417272）
 * - 尾部签名（0xAA550000）
 */
fat32_error_t
fat32_boot_validate_fsinfo(const fat32_fsinfo_t *fsinfo);

/**
 * @brief 获取文件系统类型字符串
 * 
 * 根据引导扇区信息判断文件系统类型。
 * 
 * @param boot_sector 引导扇区结构指针
 * @return const char* 文件系统类型字符串
 * 
 * 返回值：
 * - "FAT32" - 有效的FAT32文件系统
 * - "UNKNOWN" - 未知或无效的文件系统
 */
const char *
fat32_boot_get_fs_type_string(const fat32_boot_sector_t *boot_sector);

/**
 * @brief 打印引导扇区信息
 * 
 * 输出引导扇区的详细信息，用于调试和诊断。
 * 
 * @param boot_sector 引导扇区结构指针
 * 
 * 输出内容：
 * - 基本文件系统参数
 * - 磁盘布局信息
 * - 卷标和文件系统类型
 */
void
fat32_boot_print_info(const fat32_boot_sector_t *boot_sector);

/**
 * @brief 打印文件系统布局信息
 * 
 * 输出计算得出的文件系统布局参数。
 * 
 * @param fs_info 文件系统信息结构指针
 * 
 * 输出内容：
 * - 各区域的起始扇区
 * - 簇的大小和数量
 * - 空闲空间信息
 */
void
fat32_boot_print_layout(const fat32_fs_info_t *fs_info);

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */

/**
 * @brief 检查扇区大小是否有效
 * 
 * @param bytes_per_sector 每扇区字节数
 * @return uint8_t 1表示有效，0表示无效
 */
static inline uint8_t
fat32_boot_is_valid_sector_size(uint16_t bytes_per_sector)
{
    return (bytes_per_sector == 512 || bytes_per_sector == 1024 || bytes_per_sector == 2048 ||
            bytes_per_sector == 4096);
}

/**
 * @brief 检查每簇扇区数是否有效
 * 
 * @param sectors_per_cluster 每簇扇区数
 * @return uint8_t 1表示有效，0表示无效
 */
static inline uint8_t
fat32_boot_is_valid_cluster_size(uint8_t sectors_per_cluster)
{
    // 必须是2的幂，且在合理范围内
    return (sectors_per_cluster > 0 && (sectors_per_cluster & (sectors_per_cluster - 1)) == 0 &&
            sectors_per_cluster <= 128);
}

/**
 * @brief 计算簇号对应的第一个扇区
 * 
 * @param fs_info 文件系统信息结构指针
 * @param cluster_num 簇号
 * @return uint32_t 扇区号
 */
static inline uint32_t
fat32_boot_cluster_to_sector(const fat32_fs_info_t *fs_info, uint32_t cluster_num)
{
    if (cluster_num < 2) {
        return 0;  // 无效簇号
    }
    return fs_info->data_start_sector + (cluster_num - 2) * fs_info->sectors_per_cluster;
}

/**
 * @brief 计算扇区号对应的簇号
 * 
 * @param fs_info 文件系统信息结构指针
 * @param sector_num 扇区号
 * @return uint32_t 簇号
 */
static inline uint32_t
fat32_boot_sector_to_cluster(const fat32_fs_info_t *fs_info, uint32_t sector_num)
{
    if (sector_num < fs_info->data_start_sector) {
        return 0;  // 不在数据区
    }
    return ((sector_num - fs_info->data_start_sector) / fs_info->sectors_per_cluster) + 2;
}

#endif  // FAT32_BOOT_H
