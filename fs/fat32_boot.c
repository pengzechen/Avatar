/**
 * @file fat32_boot.c
 * @brief FAT32引导扇区管理实现
 * 
 * 本文件实现了FAT32引导扇区的读取、解析和验证功能。
 */

#include "fs/fat32_boot.h"
#include "lib/avatar_string.h"
#include "lib/avatar_assert.h"
#include "io.h"

/* ============================================================================
 * 公共函数实现
 * ============================================================================ */

fat32_error_t fat32_boot_read_boot_sector(fat32_disk_t *disk, fat32_fs_info_t *fs_info)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    
    // 读取引导扇区
    fat32_error_t result = fat32_disk_read_sectors(disk, 0, 1, &fs_info->boot_sector);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to read boot sector\n");
        return result;
    }
    
    // 验证引导扇区
    result = fat32_boot_validate_boot_sector(&fs_info->boot_sector);
    if (result != FAT32_OK) {
        logger("FAT32: Invalid boot sector\n");
        return result;
    }
    
    // 计算文件系统布局
    result = fat32_boot_calculate_layout(&fs_info->boot_sector, fs_info);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to calculate filesystem layout\n");
        return result;
    }
    
    // 读取FSInfo结构
    result = fat32_boot_read_fsinfo(disk, fs_info);
    if (result != FAT32_OK) {
        logger("FAT32: Warning - Failed to read FSInfo, using defaults\n");
        // FSInfo读取失败不是致命错误，使用默认值
        fs_info->free_cluster_count = 0xFFFFFFFF;  // 未知
        fs_info->next_free_cluster = 2;
    }
    
    fs_info->mounted = 1;
    
    logger("FAT32: Boot sector read and validated successfully\n");
    return FAT32_OK;
}

fat32_error_t fat32_boot_validate_boot_sector(const fat32_boot_sector_t *boot_sector)
{
    avatar_assert(boot_sector != NULL);
    
    // 检查引导扇区签名
    if (boot_sector->boot_sector_signature != 0xAA55) {
        logger("FAT32: Invalid boot sector signature: 0x%04X\n", 
               boot_sector->boot_sector_signature);
        return FAT32_ERROR_CORRUPTED;
    }
    
    // 检查扇区大小
    if (!fat32_boot_is_valid_sector_size(boot_sector->bytes_per_sector)) {
        logger("FAT32: Invalid sector size: %u\n", boot_sector->bytes_per_sector);
        return FAT32_ERROR_CORRUPTED;
    }
    
    // 检查每簇扇区数
    if (!fat32_boot_is_valid_cluster_size(boot_sector->sectors_per_cluster)) {
        logger("FAT32: Invalid cluster size: %u sectors\n", 
               boot_sector->sectors_per_cluster);
        return FAT32_ERROR_CORRUPTED;
    }
    
    // 检查FAT表数量
    if (boot_sector->num_fats == 0 || boot_sector->num_fats > 2) {
        logger("FAT32: Invalid number of FATs: %u\n", boot_sector->num_fats);
        return FAT32_ERROR_CORRUPTED;
    }
    
    // 检查保留扇区数
    if (boot_sector->reserved_sectors == 0) {
        logger("FAT32: Invalid reserved sectors: %u\n", boot_sector->reserved_sectors);
        return FAT32_ERROR_CORRUPTED;
    }
    
    // 检查FAT32特有字段
    if (boot_sector->fat_size_32 == 0) {
        logger("FAT32: Invalid FAT size: %u\n", boot_sector->fat_size_32);
        return FAT32_ERROR_CORRUPTED;
    }
    
    if (boot_sector->root_cluster < 2) {
        logger("FAT32: Invalid root cluster: %u\n", boot_sector->root_cluster);
        return FAT32_ERROR_CORRUPTED;
    }
    
    // 检查文件系统类型字符串（可选）
    if (memcmp(boot_sector->fs_type, "FAT32   ", 8) != 0) {
        logger("FAT32: Warning - FS type string mismatch\n");
        // 这不是致命错误，继续处理
    }
    
    return FAT32_OK;
}

fat32_error_t fat32_boot_calculate_layout(const fat32_boot_sector_t *boot_sector, 
                                          fat32_fs_info_t *fs_info)
{
    avatar_assert(boot_sector != NULL);
    avatar_assert(fs_info != NULL);
    
    // 计算FAT表起始扇区
    fs_info->fat_start_sector = boot_sector->reserved_sectors;
    
    // 计算数据区起始扇区
    fs_info->data_start_sector = boot_sector->reserved_sectors + 
                                 boot_sector->num_fats * boot_sector->fat_size_32;
    
    // 计算总扇区数
    uint32_t total_sectors = boot_sector->total_sectors_32;
    if (total_sectors == 0) {
        total_sectors = boot_sector->total_sectors_16;
    }
    
    // 计算数据区扇区数
    uint32_t data_sectors = total_sectors - fs_info->data_start_sector;
    
    // 计算总簇数
    fs_info->total_clusters = data_sectors / boot_sector->sectors_per_cluster;
    
    // 验证这确实是FAT32（簇数必须大于65525）
    if (fs_info->total_clusters <= 65525) {
        logger("FAT32: Cluster count %u indicates this is not FAT32\n", 
               fs_info->total_clusters);
        return FAT32_ERROR_CORRUPTED;
    }
    
    // 设置簇相关参数
    fs_info->sectors_per_cluster = boot_sector->sectors_per_cluster;
    fs_info->bytes_per_cluster = boot_sector->sectors_per_cluster * boot_sector->bytes_per_sector;
    
    logger("FAT32: Layout calculated - FAT starts at sector %u, data starts at sector %u, %u clusters\n",
           fs_info->fat_start_sector, fs_info->data_start_sector, fs_info->total_clusters);
    
    return FAT32_OK;
}

fat32_error_t fat32_boot_read_fsinfo(fat32_disk_t *disk, fat32_fs_info_t *fs_info)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    
    uint16_t fsinfo_sector = fs_info->boot_sector.fs_info;
    if (fsinfo_sector == 0) {
        logger("FAT32: No FSInfo sector specified\n");
        return FAT32_ERROR_NOT_FOUND;
    }
    
    // 读取FSInfo扇区
    fat32_error_t result = fat32_disk_read_sectors(disk, fsinfo_sector, 1, &fs_info->fsinfo);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to read FSInfo sector %u\n", fsinfo_sector);
        return result;
    }
    
    // 验证FSInfo结构
    result = fat32_boot_validate_fsinfo(&fs_info->fsinfo);
    if (result != FAT32_OK) {
        logger("FAT32: Invalid FSInfo structure\n");
        return result;
    }
    
    // 提取空闲簇信息
    if (fs_info->fsinfo.free_count != 0xFFFFFFFF) {
        fs_info->free_cluster_count = fs_info->fsinfo.free_count;
    } else {
        fs_info->free_cluster_count = 0xFFFFFFFF;  // 未知
    }
    
    if (fs_info->fsinfo.next_free != 0xFFFFFFFF && fs_info->fsinfo.next_free >= 2) {
        fs_info->next_free_cluster = fs_info->fsinfo.next_free;
    } else {
        fs_info->next_free_cluster = 2;  // 从簇2开始
    }
    
    logger("FAT32: FSInfo read - free clusters: %u, next free: %u\n",
           fs_info->free_cluster_count, fs_info->next_free_cluster);
    
    return FAT32_OK;
}

fat32_error_t fat32_boot_write_fsinfo(fat32_disk_t *disk, const fat32_fs_info_t *fs_info)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    
    uint16_t fsinfo_sector = fs_info->boot_sector.fs_info;
    if (fsinfo_sector == 0) {
        return FAT32_ERROR_NOT_FOUND;
    }
    
    fat32_fsinfo_t fsinfo = fs_info->fsinfo;
    
    // 更新空闲簇信息
    fsinfo.free_count = fs_info->free_cluster_count;
    fsinfo.next_free = fs_info->next_free_cluster;
    
    // 写入FSInfo扇区
    fat32_error_t result = fat32_disk_write_sectors(disk, fsinfo_sector, 1, &fsinfo);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to write FSInfo sector %u\n", fsinfo_sector);
        return result;
    }
    
    return FAT32_OK;
}

fat32_error_t fat32_boot_validate_fsinfo(const fat32_fsinfo_t *fsinfo)
{
    avatar_assert(fsinfo != NULL);
    
    // 检查签名
    if (fsinfo->lead_signature != 0x41615252) {
        logger("FAT32: Invalid FSInfo lead signature: 0x%08X\n", fsinfo->lead_signature);
        return FAT32_ERROR_CORRUPTED;
    }
    
    if (fsinfo->struct_signature != 0x61417272) {
        logger("FAT32: Invalid FSInfo struct signature: 0x%08X\n", fsinfo->struct_signature);
        return FAT32_ERROR_CORRUPTED;
    }
    
    if (fsinfo->trail_signature != 0xAA550000) {
        logger("FAT32: Invalid FSInfo trail signature: 0x%08X\n", fsinfo->trail_signature);
        return FAT32_ERROR_CORRUPTED;
    }
    
    return FAT32_OK;
}

const char *fat32_boot_get_fs_type_string(const fat32_boot_sector_t *boot_sector)
{
    avatar_assert(boot_sector != NULL);

    if (memcmp(boot_sector->fs_type, "FAT32   ", 8) == 0) {
        return "FAT32";
    }

    return "UNKNOWN";
}

void fat32_boot_print_info(const fat32_boot_sector_t *boot_sector)
{
    avatar_assert(boot_sector != NULL);

    logger("=== FAT32 Boot Sector Information ===\n");
    logger("OEM Name: %.8s\n", boot_sector->oem_name);
    logger("Bytes per sector: %u\n", boot_sector->bytes_per_sector);
    logger("Sectors per cluster: %u\n", boot_sector->sectors_per_cluster);
    logger("Reserved sectors: %u\n", boot_sector->reserved_sectors);
    logger("Number of FATs: %u\n", boot_sector->num_fats);
    logger("Total sectors: %u\n",
           boot_sector->total_sectors_32 ? boot_sector->total_sectors_32 : boot_sector->total_sectors_16);
    logger("FAT size (sectors): %u\n", boot_sector->fat_size_32);
    logger("Root cluster: %u\n", boot_sector->root_cluster);
    logger("FSInfo sector: %u\n", boot_sector->fs_info);
    logger("Volume label: %.11s\n", boot_sector->volume_label);
    logger("File system type: %.8s\n", boot_sector->fs_type);
    logger("Boot signature: 0x%04X\n", boot_sector->boot_sector_signature);
    logger("=====================================\n");
}

void fat32_boot_print_layout(const fat32_fs_info_t *fs_info)
{
    avatar_assert(fs_info != NULL);

    logger("=== FAT32 Filesystem Layout ===\n");
    logger("FAT start sector: %u\n", fs_info->fat_start_sector);
    logger("Data start sector: %u\n", fs_info->data_start_sector);
    logger("Total clusters: %u\n", fs_info->total_clusters);
    logger("Sectors per cluster: %u\n", fs_info->sectors_per_cluster);
    logger("Bytes per cluster: %u\n", fs_info->bytes_per_cluster);
    logger("Free clusters: %u\n",
           fs_info->free_cluster_count == 0xFFFFFFFF ? 0 : fs_info->free_cluster_count);
    logger("Next free cluster: %u\n", fs_info->next_free_cluster);
    logger("Mounted: %s\n", fs_info->mounted ? "Yes" : "No");
    logger("===============================\n");
}
