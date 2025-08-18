/**
 * @file fat32_fat.c
 * @brief FAT32文件分配表管理实现
 * 
 * 本文件实现了FAT32文件分配表的管理功能。
 */

#include "fs/fat32_fat.h"
#include "lib/avatar_string.h"
#include "lib/avatar_assert.h"
#include "mem/mem.h"
#include "io.h"

/* ============================================================================
 * FAT表操作函数实现
 * ============================================================================ */

fat32_error_t fat32_fat_read_entry(fat32_disk_t *disk, 
                                   const fat32_fs_info_t *fs_info,
                                   uint32_t cluster_num, 
                                   uint32_t *fat_entry)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(fat_entry != NULL);
    
    if (!fat32_fat_is_valid_cluster(fs_info, cluster_num)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 计算FAT表项所在的扇区和偏移
    uint32_t fat_sector = fat32_fat_get_entry_sector(fs_info, cluster_num);
    uint32_t fat_offset = fat32_fat_get_entry_offset(cluster_num);
    
    // 读取扇区数据
    uint8_t sector_buffer[FAT32_SECTOR_SIZE];
    fat32_error_t result = fat32_disk_read_sectors(disk, fat_sector, 1, sector_buffer);
    if (result != FAT32_OK) {
        return result;
    }
    
    // 提取FAT表项值（小端序）
    *fat_entry = *(uint32_t *)(sector_buffer + fat_offset) & 0x0FFFFFFF;
    
    return FAT32_OK;
}

fat32_error_t fat32_fat_write_entry(fat32_disk_t *disk, 
                                    const fat32_fs_info_t *fs_info,
                                    uint32_t cluster_num, 
                                    uint32_t fat_entry)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    
    if (!fat32_fat_is_valid_cluster(fs_info, cluster_num)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 计算FAT表项所在的扇区和偏移
    uint32_t fat_sector = fat32_fat_get_entry_sector(fs_info, cluster_num);
    uint32_t fat_offset = fat32_fat_get_entry_offset(cluster_num);
    
    // 读取扇区数据
    uint8_t sector_buffer[FAT32_SECTOR_SIZE];
    fat32_error_t result = fat32_disk_read_sectors(disk, fat_sector, 1, sector_buffer);
    if (result != FAT32_OK) {
        return result;
    }
    
    // 修改FAT表项值（保留高4位，只修改低28位）
    uint32_t *entry_ptr = (uint32_t *)(sector_buffer + fat_offset);
    *entry_ptr = (*entry_ptr & 0xF0000000) | (fat_entry & 0x0FFFFFFF);
    
    // 写入所有FAT表副本
    for (uint8_t fat_num = 0; fat_num < fs_info->boot_sector.num_fats; fat_num++) {
        uint32_t fat_start = fs_info->fat_start_sector + fat_num * fs_info->boot_sector.fat_size_32;
        uint32_t target_sector = fat_start + (fat_sector - fs_info->fat_start_sector);
        
        result = fat32_disk_write_sectors(disk, target_sector, 1, sector_buffer);
        if (result != FAT32_OK) {
            logger("FAT32: Failed to write FAT %u sector %u\n", fat_num, target_sector);
            return result;
        }
    }
    
    return FAT32_OK;
}

/* ============================================================================
 * 簇分配和释放函数实现
 * ============================================================================ */

fat32_error_t fat32_fat_allocate_cluster(fat32_disk_t *disk, 
                                         fat32_fs_info_t *fs_info,
                                         uint32_t *cluster_num)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(cluster_num != NULL);
    
    uint32_t start_cluster = fs_info->next_free_cluster;
    uint32_t current_cluster = start_cluster;
    uint32_t fat_entry;
    
    // 从next_free_cluster开始查找空闲簇
    do {
        fat32_error_t result = fat32_fat_read_entry(disk, fs_info, current_cluster, &fat_entry);
        if (result != FAT32_OK) {
            return result;
        }
        
        if (fat32_fat_is_free_cluster(fat_entry)) {
            // 找到空闲簇，标记为簇链结束
            result = fat32_fat_write_entry(disk, fs_info, current_cluster, FAT32_EOC_MAX);
            if (result != FAT32_OK) {
                return result;
            }
            
            *cluster_num = current_cluster;
            
            // 更新FSInfo信息
            if (fs_info->free_cluster_count != 0xFFFFFFFF) {
                fs_info->free_cluster_count--;
            }
            
            // 更新下一个空闲簇提示
            fs_info->next_free_cluster = current_cluster + 1;
            if (fs_info->next_free_cluster >= fs_info->total_clusters + 2) {
                fs_info->next_free_cluster = 2;
            }
            
            return FAT32_OK;
        }
        
        // 移动到下一个簇
        current_cluster++;
        if (current_cluster >= fs_info->total_clusters + 2) {
            current_cluster = 2;
        }
        
    } while (current_cluster != start_cluster);
    
    // 没有找到空闲簇
    return FAT32_ERROR_NO_SPACE;
}

fat32_error_t fat32_fat_free_cluster(fat32_disk_t *disk, 
                                     fat32_fs_info_t *fs_info,
                                     uint32_t cluster_num)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    
    if (!fat32_fat_is_valid_cluster(fs_info, cluster_num)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 将簇标记为空闲
    fat32_error_t result = fat32_fat_write_entry(disk, fs_info, cluster_num, FAT32_FREE_CLUSTER);
    if (result != FAT32_OK) {
        return result;
    }
    
    // 更新FSInfo信息
    if (fs_info->free_cluster_count != 0xFFFFFFFF) {
        fs_info->free_cluster_count++;
    }
    
    // 如果释放的簇号小于next_free_cluster，则更新next_free_cluster
    if (cluster_num < fs_info->next_free_cluster) {
        fs_info->next_free_cluster = cluster_num;
    }
    
    return FAT32_OK;
}

fat32_error_t fat32_fat_allocate_cluster_chain(fat32_disk_t *disk, 
                                               fat32_fs_info_t *fs_info,
                                               uint32_t cluster_count,
                                               uint32_t *first_cluster)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(first_cluster != NULL);
    avatar_assert(cluster_count > 0);
    
    if (cluster_count == 1) {
        return fat32_fat_allocate_cluster(disk, fs_info, first_cluster);
    }
    
    // 分配第一个簇
    fat32_error_t result = fat32_fat_allocate_cluster(disk, fs_info, first_cluster);
    if (result != FAT32_OK) {
        return result;
    }
    
    uint32_t prev_cluster = *first_cluster;
    
    // 分配剩余的簇并连接成链
    for (uint32_t i = 1; i < cluster_count; i++) {
        uint32_t new_cluster;
        result = fat32_fat_allocate_cluster(disk, fs_info, &new_cluster);
        if (result != FAT32_OK) {
            // 分配失败，释放已分配的簇
            fat32_fat_free_cluster_chain(disk, fs_info, *first_cluster);
            return result;
        }
        
        // 连接到簇链
        result = fat32_fat_write_entry(disk, fs_info, prev_cluster, new_cluster);
        if (result != FAT32_OK) {
            // 连接失败，释放已分配的簇
            fat32_fat_free_cluster(disk, fs_info, new_cluster);
            fat32_fat_free_cluster_chain(disk, fs_info, *first_cluster);
            return result;
        }
        
        prev_cluster = new_cluster;
    }
    
    return FAT32_OK;
}

fat32_error_t fat32_fat_free_cluster_chain(fat32_disk_t *disk, 
                                           fat32_fs_info_t *fs_info,
                                           uint32_t first_cluster)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    
    if (!fat32_fat_is_valid_cluster(fs_info, first_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    uint32_t current_cluster = first_cluster;
    
    while (fat32_fat_is_valid_cluster(fs_info, current_cluster)) {
        uint32_t next_cluster;
        fat32_error_t result = fat32_fat_get_next_cluster(disk, fs_info, current_cluster, &next_cluster);
        if (result != FAT32_OK) {
            return result;
        }
        
        // 释放当前簇
        result = fat32_fat_free_cluster(disk, fs_info, current_cluster);
        if (result != FAT32_OK) {
            return result;
        }
        
        // 移动到下一个簇
        if (next_cluster == 0) {
            break;  // 簇链结束
        }
        current_cluster = next_cluster;
    }
    
    return FAT32_OK;
}

/* ============================================================================
 * 簇链操作函数实现
 * ============================================================================ */

fat32_error_t fat32_fat_get_next_cluster(fat32_disk_t *disk,
                                         const fat32_fs_info_t *fs_info,
                                         uint32_t current_cluster,
                                         uint32_t *next_cluster)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(next_cluster != NULL);

    if (!fat32_fat_is_valid_cluster(fs_info, current_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    uint32_t fat_entry;
    fat32_error_t result = fat32_fat_read_entry(disk, fs_info, current_cluster, &fat_entry);
    if (result != FAT32_OK) {
        return result;
    }

    if (fat32_fat_is_bad_cluster(fat_entry)) {
        return FAT32_ERROR_CORRUPTED;
    }

    if (fat32_fat_is_end_of_chain(fat_entry)) {
        *next_cluster = 0;  // 簇链结束
    } else if (fat32_fat_is_valid_cluster(fs_info, fat_entry)) {
        *next_cluster = fat_entry;
    } else {
        return FAT32_ERROR_CORRUPTED;
    }

    return FAT32_OK;
}

fat32_error_t fat32_fat_extend_cluster_chain(fat32_disk_t *disk,
                                             fat32_fs_info_t *fs_info,
                                             uint32_t last_cluster,
                                             uint32_t *new_cluster)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(new_cluster != NULL);

    if (!fat32_fat_is_valid_cluster(fs_info, last_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    // 分配新簇
    fat32_error_t result = fat32_fat_allocate_cluster(disk, fs_info, new_cluster);
    if (result != FAT32_OK) {
        return result;
    }

    // 将last_cluster指向新簇
    result = fat32_fat_write_entry(disk, fs_info, last_cluster, *new_cluster);
    if (result != FAT32_OK) {
        // 分配失败，释放新簇
        fat32_fat_free_cluster(disk, fs_info, *new_cluster);
        return result;
    }

    return FAT32_OK;
}

fat32_error_t fat32_fat_get_cluster_chain_length(fat32_disk_t *disk,
                                                 const fat32_fs_info_t *fs_info,
                                                 uint32_t first_cluster,
                                                 uint32_t *chain_length)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(chain_length != NULL);

    if (!fat32_fat_is_valid_cluster(fs_info, first_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    uint32_t current_cluster = first_cluster;
    uint32_t length = 0;

    while (fat32_fat_is_valid_cluster(fs_info, current_cluster)) {
        length++;

        uint32_t next_cluster;
        fat32_error_t result = fat32_fat_get_next_cluster(disk, fs_info, current_cluster, &next_cluster);
        if (result != FAT32_OK) {
            return result;
        }

        if (next_cluster == 0) {
            break;  // 簇链结束
        }

        current_cluster = next_cluster;

        // 防止无限循环（检测簇链损坏）
        if (length > fs_info->total_clusters) {
            return FAT32_ERROR_CORRUPTED;
        }
    }

    *chain_length = length;
    return FAT32_OK;
}

fat32_error_t fat32_fat_get_cluster_at_index(fat32_disk_t *disk,
                                             const fat32_fs_info_t *fs_info,
                                             uint32_t first_cluster,
                                             uint32_t cluster_index,
                                             uint32_t *target_cluster)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(target_cluster != NULL);

    if (!fat32_fat_is_valid_cluster(fs_info, first_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    uint32_t current_cluster = first_cluster;
    uint32_t current_index = 0;

    while (fat32_fat_is_valid_cluster(fs_info, current_cluster)) {
        if (current_index == cluster_index) {
            *target_cluster = current_cluster;
            return FAT32_OK;
        }

        uint32_t next_cluster;
        fat32_error_t result = fat32_fat_get_next_cluster(disk, fs_info, current_cluster, &next_cluster);
        if (result != FAT32_OK) {
            return result;
        }

        if (next_cluster == 0) {
            break;  // 簇链结束，索引超出范围
        }

        current_cluster = next_cluster;
        current_index++;

        // 防止无限循环
        if (current_index > fs_info->total_clusters) {
            return FAT32_ERROR_CORRUPTED;
        }
    }

    return FAT32_ERROR_END_OF_FILE;
}
