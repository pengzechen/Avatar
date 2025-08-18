/**
 * @file fat32_disk.c
 * @brief FAT32磁盘操作抽象层实现
 * 
 * 本文件实现了FAT32文件系统的底层磁盘访问功能。
 * 使用内存模拟磁盘，提供标准的扇区读写接口。
 */

#include "fs/fat32_disk.h"
#include "mem/mem.h"
#include "lib/avatar_string.h"
#include "lib/avatar_assert.h"
#include "io.h"

/* ============================================================================
 * 全局变量
 * ============================================================================ */

static fat32_disk_t g_fat32_disk;  // 全局磁盘实例

/* ============================================================================
 * 私有函数声明
 * ============================================================================ */

static fat32_error_t fat32_disk_create_boot_sector(fat32_disk_t *disk, const char *volume_label);
static fat32_error_t fat32_disk_create_fsinfo(fat32_disk_t *disk);
static fat32_error_t fat32_disk_create_fat_tables(fat32_disk_t *disk);
static fat32_error_t fat32_disk_create_root_directory(fat32_disk_t *disk);

/* ============================================================================
 * 公共函数实现
 * ============================================================================ */

fat32_error_t fat32_disk_init(fat32_disk_t *disk)
{
    avatar_assert(disk != NULL);
    
    // 清零磁盘结构
    memset(disk, 0, sizeof(fat32_disk_t));
    
    // 分配磁盘内存空间
    disk->disk_data = (uint8_t *)kalloc_pages(FAT32_DISK_SIZE / PAGE_SIZE);
    if (disk->disk_data == NULL) {
        logger("FAT32: Failed to allocate disk memory\n");
        return FAT32_ERROR_DISK_ERROR;
    }
    
    // 清零磁盘数据
    memset(disk->disk_data, 0, FAT32_DISK_SIZE);
    
    // 设置磁盘参数
    disk->disk_size = FAT32_DISK_SIZE;
    disk->total_sectors = FAT32_TOTAL_SECTORS;
    disk->initialized = 1;
    disk->formatted = 0;
    
    // 重置统计信息
    disk->read_count = 0;
    disk->write_count = 0;
    disk->error_count = 0;
    
    logger("FAT32: Virtual disk initialized, size=%u KB, sectors=%u\n", 
           FAT32_DISK_SIZE / 1024, FAT32_TOTAL_SECTORS);
    
    return FAT32_OK;
}

fat32_error_t fat32_disk_cleanup(fat32_disk_t *disk)
{
    avatar_assert(disk != NULL);
    
    if (disk->disk_data != NULL) {
        kfree_pages(disk->disk_data, FAT32_DISK_SIZE / PAGE_SIZE);
        disk->disk_data = NULL;
    }
    
    memset(disk, 0, sizeof(fat32_disk_t));
    
    logger("FAT32: Virtual disk cleaned up\n");
    return FAT32_OK;
}

fat32_error_t fat32_disk_read_sectors(fat32_disk_t *disk, 
                                      uint32_t sector_num, 
                                      uint32_t sector_count, 
                                      void *buffer)
{
    avatar_assert(disk != NULL);
    avatar_assert(buffer != NULL);
    avatar_assert(sector_count > 0);
    
    if (!disk->initialized) {
        disk->error_count++;
        return FAT32_ERROR_DISK_ERROR;
    }
    
    // 检查扇区范围
    if (sector_num + sector_count > disk->total_sectors) {
        logger("FAT32: Read beyond disk boundary: sector %u + %u > %u\n", 
               sector_num, sector_count, disk->total_sectors);
        disk->error_count++;
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 计算偏移并复制数据
    uint32_t offset = fat32_disk_sector_to_offset(sector_num);
    uint32_t size = sector_count * FAT32_SECTOR_SIZE;
    
    memcpy(buffer, disk->disk_data + offset, size);
    
    disk->read_count++;
    
    return FAT32_OK;
}

fat32_error_t fat32_disk_write_sectors(fat32_disk_t *disk, 
                                       uint32_t sector_num, 
                                       uint32_t sector_count, 
                                       const void *buffer)
{
    avatar_assert(disk != NULL);
    avatar_assert(buffer != NULL);
    avatar_assert(sector_count > 0);
    
    if (!disk->initialized) {
        disk->error_count++;
        return FAT32_ERROR_DISK_ERROR;
    }
    
    // 检查扇区范围
    if (sector_num + sector_count > disk->total_sectors) {
        logger("FAT32: Write beyond disk boundary: sector %u + %u > %u\n", 
               sector_num, sector_count, disk->total_sectors);
        disk->error_count++;
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 计算偏移并复制数据
    uint32_t offset = fat32_disk_sector_to_offset(sector_num);
    uint32_t size = sector_count * FAT32_SECTOR_SIZE;
    
    memcpy(disk->disk_data + offset, buffer, size);
    
    disk->write_count++;
    
    return FAT32_OK;
}

fat32_error_t fat32_disk_format(fat32_disk_t *disk, const char *volume_label)
{
    avatar_assert(disk != NULL);
    
    if (!disk->initialized) {
        return FAT32_ERROR_DISK_ERROR;
    }
    
    logger("FAT32: Formatting virtual disk...\n");
    
    // 清零整个磁盘
    memset(disk->disk_data, 0, disk->disk_size);
    
    // 创建引导扇区
    fat32_error_t result = fat32_disk_create_boot_sector(disk, volume_label);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to create boot sector\n");
        return result;
    }
    
    // 创建FSInfo结构
    result = fat32_disk_create_fsinfo(disk);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to create FSInfo\n");
        return result;
    }
    
    // 创建FAT表
    result = fat32_disk_create_fat_tables(disk);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to create FAT tables\n");
        return result;
    }
    
    // 创建根目录
    result = fat32_disk_create_root_directory(disk);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to create root directory\n");
        return result;
    }
    
    disk->formatted = 1;
    
    logger("FAT32: Disk formatting completed successfully\n");
    logger("FAT32: Layout - Reserved: %u sectors, FAT: %u sectors each, Data starts at sector %u\n",
           FAT32_RESERVED_SECTORS, FAT32_FAT_SIZE_SECTORS, FAT32_DATA_START_SECTOR);
    
    return FAT32_OK;
}

fat32_error_t fat32_disk_get_info(fat32_disk_t *disk, 
                                  uint32_t *total_sectors, 
                                  uint32_t *free_sectors)
{
    avatar_assert(disk != NULL);
    
    if (!disk->initialized) {
        return FAT32_ERROR_DISK_ERROR;
    }
    
    if (total_sectors != NULL) {
        *total_sectors = disk->total_sectors;
    }
    
    if (free_sectors != NULL) {
        // 简化实现：假设大部分扇区都是空闲的
        // 实际实现中需要扫描FAT表来计算准确的空闲扇区数
        *free_sectors = disk->total_sectors - FAT32_DATA_START_SECTOR - 100; // 预留一些空间
    }
    
    return FAT32_OK;
}

fat32_error_t fat32_disk_sync(fat32_disk_t *disk)
{
    avatar_assert(disk != NULL);
    
    if (!disk->initialized) {
        return FAT32_ERROR_DISK_ERROR;
    }
    
    // 在内存模拟的情况下，同步操作是空操作
    // 实际磁盘驱动中，这里会刷新缓存到物理设备
    
    return FAT32_OK;
}

uint8_t fat32_disk_is_valid_sector(fat32_disk_t *disk, uint32_t sector_num)
{
    avatar_assert(disk != NULL);
    
    return (disk->initialized && sector_num < disk->total_sectors) ? 1 : 0;
}

fat32_error_t fat32_disk_get_stats(fat32_disk_t *disk, 
                                   uint32_t *read_count, 
                                   uint32_t *write_count, 
                                   uint32_t *error_count)
{
    avatar_assert(disk != NULL);
    
    if (read_count != NULL) {
        *read_count = disk->read_count;
    }
    
    if (write_count != NULL) {
        *write_count = disk->write_count;
    }
    
    if (error_count != NULL) {
        *error_count = disk->error_count;
    }
    
    return FAT32_OK;
}

/* ============================================================================
 * 全局磁盘实例访问函数
 * ============================================================================ */

fat32_disk_t *fat32_get_disk(void)
{
    return &g_fat32_disk;
}

/* ============================================================================
 * 私有函数实现
 * ============================================================================ */

static fat32_error_t fat32_disk_create_boot_sector(fat32_disk_t *disk, const char *volume_label)
{
    fat32_boot_sector_t boot_sector;
    memset(&boot_sector, 0, sizeof(boot_sector));

    // 设置跳转指令
    boot_sector.jmp_boot[0] = 0xEB;
    boot_sector.jmp_boot[1] = 0x58;
    boot_sector.jmp_boot[2] = 0x90;

    // 设置OEM名称
    memcpy(boot_sector.oem_name, "AVATAR  ", 8);

    // 设置基本参数
    boot_sector.bytes_per_sector = FAT32_SECTOR_SIZE;
    boot_sector.sectors_per_cluster = FAT32_SECTORS_PER_CLUSTER;
    boot_sector.reserved_sectors = FAT32_RESERVED_SECTORS;
    boot_sector.num_fats = FAT32_NUM_FATS;
    boot_sector.root_entries = 0;  // FAT32中根目录项数为0
    boot_sector.total_sectors_16 = 0;  // 使用32位字段
    boot_sector.media_type = 0xF8;  // 硬盘
    boot_sector.fat_size_16 = 0;  // FAT32中使用32位字段
    boot_sector.sectors_per_track = 63;
    boot_sector.num_heads = 255;
    boot_sector.hidden_sectors = 0;
    boot_sector.total_sectors_32 = FAT32_TOTAL_SECTORS;

    // FAT32特有字段
    boot_sector.fat_size_32 = FAT32_FAT_SIZE_SECTORS;
    boot_sector.ext_flags = 0;
    boot_sector.fs_version = 0;
    boot_sector.root_cluster = FAT32_ROOT_CLUSTER;
    boot_sector.fs_info = 1;  // FSInfo在扇区1
    boot_sector.backup_boot_sector = 6;  // 备份引导扇区在扇区6
    boot_sector.drive_number = 0x80;  // 硬盘
    boot_sector.boot_signature = 0x29;
    boot_sector.volume_id = 0x12345678;  // 简化的卷ID

    // 设置卷标
    if (volume_label != NULL) {
        size_t label_len = strlen(volume_label);
        if (label_len > 11) label_len = 11;
        memcpy(boot_sector.volume_label, volume_label, label_len);
        // 用空格填充剩余部分
        for (size_t i = label_len; i < 11; i++) {
            boot_sector.volume_label[i] = ' ';
        }
    } else {
        memcpy(boot_sector.volume_label, "NO NAME    ", 11);
    }

    // 设置文件系统类型
    memcpy(boot_sector.fs_type, "FAT32   ", 8);

    // 设置引导扇区签名
    boot_sector.boot_sector_signature = 0xAA55;

    // 写入引导扇区
    return fat32_disk_write_sectors(disk, 0, 1, &boot_sector);
}

static fat32_error_t fat32_disk_create_fsinfo(fat32_disk_t *disk)
{
    fat32_fsinfo_t fsinfo;
    memset(&fsinfo, 0, sizeof(fsinfo));

    // 设置签名
    fsinfo.lead_signature = 0x41615252;
    fsinfo.struct_signature = 0x61417272;
    fsinfo.trail_signature = 0xAA550000;

    // 设置空闲簇信息（简化计算）
    fsinfo.free_count = FAT32_CLUSTERS_COUNT - 2;  // 减去根目录占用的簇
    fsinfo.next_free = 3;  // 从簇3开始查找空闲簇

    // 写入FSInfo结构
    return fat32_disk_write_sectors(disk, 1, 1, &fsinfo);
}

static fat32_error_t fat32_disk_create_fat_tables(fat32_disk_t *disk)
{
    // 分配FAT表缓冲区
    uint32_t fat_size_bytes = FAT32_FAT_SIZE_SECTORS * FAT32_SECTOR_SIZE;
    uint32_t *fat_table = (uint32_t *)kalloc_pages((fat_size_bytes + PAGE_SIZE - 1) / PAGE_SIZE);
    if (fat_table == NULL) {
        return FAT32_ERROR_DISK_ERROR;
    }

    // 清零FAT表
    memset(fat_table, 0, fat_size_bytes);

    // 设置特殊簇值
    fat_table[0] = 0x0FFFFFF8;  // 媒体类型 + 脏位
    fat_table[1] = 0x0FFFFFFF;  // 簇链结束标记
    fat_table[2] = 0x0FFFFFFF;  // 根目录簇链结束标记

    // 写入第一个FAT表
    fat32_error_t result = fat32_disk_write_sectors(disk, FAT32_RESERVED_SECTORS,
                                                    FAT32_FAT_SIZE_SECTORS, fat_table);
    if (result != FAT32_OK) {
        kfree_pages(fat_table, (fat_size_bytes + PAGE_SIZE - 1) / PAGE_SIZE);
        return result;
    }

    // 写入第二个FAT表（备份）
    result = fat32_disk_write_sectors(disk, FAT32_RESERVED_SECTORS + FAT32_FAT_SIZE_SECTORS,
                                      FAT32_FAT_SIZE_SECTORS, fat_table);

    kfree_pages(fat_table, (fat_size_bytes + PAGE_SIZE - 1) / PAGE_SIZE);
    return result;
}

static fat32_error_t fat32_disk_create_root_directory(fat32_disk_t *disk)
{
    // 根目录占用一个簇，清零即可
    uint32_t cluster_size = FAT32_SECTORS_PER_CLUSTER * FAT32_SECTOR_SIZE;
    uint8_t *cluster_data = (uint8_t *)kalloc_pages((cluster_size + PAGE_SIZE - 1) / PAGE_SIZE);
    if (cluster_data == NULL) {
        return FAT32_ERROR_DISK_ERROR;
    }

    memset(cluster_data, 0, cluster_size);

    // 计算根目录的扇区位置
    uint32_t root_sector = FAT32_DATA_START_SECTOR + (FAT32_ROOT_CLUSTER - 2) * FAT32_SECTORS_PER_CLUSTER;

    fat32_error_t result = fat32_disk_write_sectors(disk, root_sector,
                                                    FAT32_SECTORS_PER_CLUSTER, cluster_data);

    kfree_pages(cluster_data, (cluster_size + PAGE_SIZE - 1) / PAGE_SIZE);
    return result;
}
