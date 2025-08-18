/**
 * @file fat32_dir.c
 * @brief FAT32目录操作实现
 * 
 * 本文件实现了FAT32目录的操作功能。
 */

#include "fs/fat32_dir.h"
#include "fs/fat32_fat.h"
#include "fs/fat32_boot.h"
#include "lib/avatar_string.h"
#include "lib/avatar_assert.h"
#include "mem/mem.h"
#include "io.h"

/* ============================================================================
 * 私有函数声明
 * ============================================================================ */

static fat32_error_t fat32_dir_read_cluster_data(fat32_disk_t *disk,
                                                 const fat32_fs_info_t *fs_info,
                                                 uint32_t cluster_num,
                                                 uint8_t *buffer);

static fat32_error_t fat32_dir_write_cluster_data(fat32_disk_t *disk,
                                                  const fat32_fs_info_t *fs_info,
                                                  uint32_t cluster_num,
                                                  const uint8_t *buffer);

static fat32_error_t fat32_dir_find_free_entry(fat32_disk_t *disk,
                                               fat32_fs_info_t *fs_info,
                                               uint32_t dir_cluster,
                                               uint32_t *entry_index);

/* ============================================================================
 * 目录操作函数实现
 * ============================================================================ */
/*
dir_cluster 表示 目录的起始簇号。告诉函数“我操作哪个目录”，从这个簇开始沿着簇链查找目录项。
entry_index 目录项在 目录文件 中的索引号（从 0 开始编号）。

有了 dir_cluster 和 entry_index，你就能唯一定位到 目录中的某个具体目录项（directory entry）。

fat32_dir_entry_t => 一个文件(可能是真实文件,也可能是目录文件)
*/

fat32_error_t fat32_dir_read_entry(fat32_disk_t *disk,
                                   const fat32_fs_info_t *fs_info,
                                   uint32_t dir_cluster,
                                   uint32_t entry_index,
                                   fat32_dir_entry_t *dir_entry)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(dir_entry != NULL);
    
    if (!fat32_fat_is_valid_cluster(fs_info, dir_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 计算目录项在哪个簇中
    uint32_t entries_per_cluster = fs_info->bytes_per_cluster / FAT32_DIR_ENTRY_SIZE;
    uint32_t cluster_index = entry_index / entries_per_cluster;
    uint32_t entry_in_cluster = entry_index % entries_per_cluster;
    
    // 找到目标簇
    uint32_t target_cluster;
    fat32_error_t result = fat32_fat_get_cluster_at_index(disk, fs_info, dir_cluster, 
                                                          cluster_index, &target_cluster);
    if (result != FAT32_OK) {
        return result;
    }
    
    // 读取簇数据
    uint8_t *cluster_buffer = (uint8_t *)kalloc_pages((fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    if (cluster_buffer == NULL) {
        return FAT32_ERROR_DISK_ERROR;
    }
    
    result = fat32_dir_read_cluster_data(disk, fs_info, target_cluster, cluster_buffer);
    if (result != FAT32_OK) {
        kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
        return result;
    }
    
    // 复制目录项数据
    memcpy(dir_entry, cluster_buffer + entry_in_cluster * FAT32_DIR_ENTRY_SIZE, FAT32_DIR_ENTRY_SIZE);
    
    kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    return FAT32_OK;
}

fat32_error_t fat32_dir_write_entry(fat32_disk_t *disk,
                                    const fat32_fs_info_t *fs_info,
                                    uint32_t dir_cluster,
                                    uint32_t entry_index,
                                    const fat32_dir_entry_t *dir_entry)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(dir_entry != NULL);
    
    if (!fat32_fat_is_valid_cluster(fs_info, dir_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 计算目录项在哪个簇中
    uint32_t entries_per_cluster = fs_info->bytes_per_cluster / FAT32_DIR_ENTRY_SIZE;
    uint32_t cluster_index = entry_index / entries_per_cluster;
    uint32_t entry_in_cluster = entry_index % entries_per_cluster;
    
    // 找到目标簇
    uint32_t target_cluster;
    fat32_error_t result = fat32_fat_get_cluster_at_index(disk, fs_info, dir_cluster, 
                                                          cluster_index, &target_cluster);
    if (result != FAT32_OK) {
        return result;
    }
    
    // 读取簇数据
    uint8_t *cluster_buffer = (uint8_t *)kalloc_pages((fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    if (cluster_buffer == NULL) {
        return FAT32_ERROR_DISK_ERROR;
    }
    
    result = fat32_dir_read_cluster_data(disk, fs_info, target_cluster, cluster_buffer);
    if (result != FAT32_OK) {
        kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
        return result;
    }
    
    // 修改目录项数据
    memcpy(cluster_buffer + entry_in_cluster * FAT32_DIR_ENTRY_SIZE, dir_entry, FAT32_DIR_ENTRY_SIZE);
    
    // 写回簇数据
    result = fat32_dir_write_cluster_data(disk, fs_info, target_cluster, cluster_buffer);
    
    kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    return result;
}

fat32_error_t fat32_dir_find_entry(fat32_disk_t *disk,
                                   const fat32_fs_info_t *fs_info,
                                   uint32_t dir_cluster,
                                   const char *filename,
                                   /* out */
                                   fat32_dir_entry_t *dir_entry,
                                   uint32_t *entry_index)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(filename != NULL);
    avatar_assert(dir_entry != NULL);
    
    if (!fat32_fat_is_valid_cluster(fs_info, dir_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 转换文件名为8.3格式
    uint8_t short_name[11];
    fat32_error_t result = fat32_dir_convert_to_short_name(filename, short_name);
    if (result != FAT32_OK) {
        return result;
    }
    
    // 遍历目录查找文件
    fat32_dir_iterator_t iterator;
    fat32_dir_iterator_init(&iterator, dir_cluster);
    
    uint32_t current_index = 0;
    while (!iterator.end_of_dir) {
        fat32_dir_entry_t current_entry;
        result = fat32_dir_iterator_next(disk, fs_info, &iterator, &current_entry);
        if (result != FAT32_OK) {
            if (result == FAT32_ERROR_END_OF_FILE) {
                break;  // 到达目录末尾
            }
            return result;
        }
        
        // 跳过空闲和已删除的目录项
        if (fat32_dir_is_free_entry(&current_entry) || fat32_dir_is_deleted_entry(&current_entry)) {
            current_index++;
            continue;
        }
        
        // 比较文件名
        if (fat32_dir_compare_short_names(current_entry.name, short_name) == 0) {
            *dir_entry = current_entry;
            if (entry_index != NULL) {
                *entry_index = current_index;
            }
            return FAT32_OK;
        }
        
        current_index++;
    }
    
    return FAT32_ERROR_NOT_FOUND;
}

fat32_error_t fat32_dir_create_entry(fat32_disk_t *disk,
                                     fat32_fs_info_t *fs_info,
                                     uint32_t dir_cluster,
                                     const char *filename,
                                     uint8_t attr,
                                     uint32_t first_cluster,
                                     uint32_t file_size,
                                     uint32_t *entry_index)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(filename != NULL);
    
    if (!fat32_fat_is_valid_cluster(fs_info, dir_cluster)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 检查文件名是否有效
    if (!fat32_dir_is_valid_filename(filename)) {
        return FAT32_ERROR_INVALID_NAME;
    }
    
    // 检查文件是否已存在
    fat32_dir_entry_t existing_entry;
    fat32_error_t result = fat32_dir_find_entry(disk, fs_info, dir_cluster, filename, 
                                                &existing_entry, NULL);
    if (result == FAT32_OK) {
        return FAT32_ERROR_ALREADY_EXISTS;
    }
    
    // 查找空闲目录项
    uint32_t free_index;
    result = fat32_dir_find_free_entry(disk, fs_info, dir_cluster, &free_index);
    if (result != FAT32_OK) {
        return result;
    }
    
    // 创建新的目录项
    fat32_dir_entry_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    
    // 转换文件名
    result = fat32_dir_convert_to_short_name(filename, new_entry.name);
    if (result != FAT32_OK) {
        return result;
    }
    
    // 设置目录项属性
    new_entry.attr = attr;
    fat32_dir_set_first_cluster(&new_entry, first_cluster);
    new_entry.file_size = file_size;
    
    // 设置时间戳（简化实现，使用固定值）
    new_entry.create_time = 0x0000;
    new_entry.create_date = 0x0021;  // 1980年1月1日
    new_entry.write_time = 0x0000;
    new_entry.write_date = 0x0021;
    new_entry.last_access_date = 0x0021;
    
    // 写入目录项
    result = fat32_dir_write_entry(disk, fs_info, dir_cluster, free_index, &new_entry);
    if (result != FAT32_OK) {
        return result;
    }
    
    if (entry_index != NULL) {
        *entry_index = free_index;
    }
    
    return FAT32_OK;
}

fat32_error_t fat32_dir_delete_entry(fat32_disk_t *disk,
                                     const fat32_fs_info_t *fs_info,
                                     uint32_t dir_cluster,
                                     uint32_t entry_index)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);

    // 读取目录项
    fat32_dir_entry_t dir_entry;
    fat32_error_t result = fat32_dir_read_entry(disk, fs_info, dir_cluster, entry_index, &dir_entry);
    if (result != FAT32_OK) {
        return result;
    }

    // 标记为已删除
    dir_entry.name[0] = FAT32_DIR_ENTRY_DELETED;

    // 写回目录项
    return fat32_dir_write_entry(disk, fs_info, dir_cluster, entry_index, &dir_entry);
}

/* ============================================================================
 * 目录遍历函数实现
 * ============================================================================ */

void fat32_dir_iterator_init(fat32_dir_iterator_t *iterator, uint32_t dir_cluster)
{
    avatar_assert(iterator != NULL);

    iterator->dir_cluster = dir_cluster;
    iterator->current_cluster = dir_cluster;
    iterator->entry_index = 0;
    iterator->cluster_offset = 0;
    iterator->end_of_dir = 0;
}

// 通过迭代器获取下一个目录项。
fat32_error_t fat32_dir_iterator_next(fat32_disk_t *disk,
                                      const fat32_fs_info_t *fs_info,
                                      fat32_dir_iterator_t *iterator,
                                      fat32_dir_entry_t *dir_entry)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(iterator != NULL);
    avatar_assert(dir_entry != NULL);

    if (iterator->end_of_dir) {
        return FAT32_ERROR_END_OF_FILE;
    }

    // 读取当前目录项
    fat32_error_t result = fat32_dir_read_entry(disk, fs_info, iterator->dir_cluster,
                                                iterator->entry_index, dir_entry);
    if (result != FAT32_OK) {
        return result;
    }

    // 检查是否到达目录末尾
    if (fat32_dir_is_free_entry(dir_entry)) {
        iterator->end_of_dir = 1;
        return FAT32_ERROR_END_OF_FILE;
    }

    // 移动到下一个目录项
    iterator->entry_index++;
    iterator->cluster_offset += FAT32_DIR_ENTRY_SIZE;

    // 检查是否需要移动到下一个簇
    if (iterator->cluster_offset >= fs_info->bytes_per_cluster) {
        uint32_t next_cluster;
        result = fat32_fat_get_next_cluster(disk, fs_info, iterator->current_cluster, &next_cluster);
        if (result != FAT32_OK) {
            return result;
        }

        if (next_cluster == 0) {
            iterator->end_of_dir = 1;
        } else {
            iterator->current_cluster = next_cluster;
            iterator->cluster_offset = 0;
        }
    }

    return FAT32_OK;
}

// 判断目录里是否有有效文件或子目录。
fat32_error_t fat32_dir_is_empty(fat32_disk_t *disk,
                                 const fat32_fs_info_t *fs_info,
                                 uint32_t dir_cluster,
                                 uint8_t *is_empty)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(is_empty != NULL);

    fat32_dir_iterator_t iterator;
    fat32_dir_iterator_init(&iterator, dir_cluster);

    *is_empty = 1;  // 假设为空

    while (!iterator.end_of_dir) {
        fat32_dir_entry_t dir_entry;
        fat32_error_t result = fat32_dir_iterator_next(disk, fs_info, &iterator, &dir_entry);
        if (result != FAT32_OK) {
            if (result == FAT32_ERROR_END_OF_FILE) {
                break;
            }
            return result;
        }

        // 跳过空闲、已删除和特殊目录项（"." 和 ".."）
        if (fat32_dir_is_free_entry(&dir_entry) ||
            fat32_dir_is_deleted_entry(&dir_entry) ||
            (dir_entry.name[0] == '.' && (dir_entry.name[1] == ' ' || dir_entry.name[1] == '.'))) {
            continue;
        }

        // 找到有效的目录项，目录不为空
        *is_empty = 0;
        break;
    }

    return FAT32_OK;
}

/* ============================================================================
 * 文件名处理函数实现
 * ============================================================================ */

fat32_error_t fat32_dir_convert_to_short_name(const char *long_name, uint8_t *short_name)
{
    avatar_assert(long_name != NULL);
    avatar_assert(short_name != NULL);

    // 清空短文件名缓冲区
    memset(short_name, ' ', 11);

    size_t name_len = strlen(long_name);
    if (name_len == 0 || name_len > 255) {
        return FAT32_ERROR_INVALID_NAME;
    }

    // 查找扩展名分隔符
    const char *dot_pos = strrchr(long_name, '.');
    size_t base_len, ext_len = 0;

    if (dot_pos != NULL && dot_pos != long_name) {
        base_len = dot_pos - long_name;
        ext_len = name_len - base_len - 1;
        if (ext_len > 3) ext_len = 3;  // 扩展名最多3个字符
    } else {
        base_len = name_len;
    }

    if (base_len > 8) base_len = 8;  // 基本名最多8个字符

    // 复制基本名（转换为大写）
    for (size_t i = 0; i < base_len; i++) {
        char c = long_name[i];
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        // 简化实现：只允许字母、数字和一些特殊字符
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '_' || c == '-' || c == '~') {
            short_name[i] = c;
        } else {
            short_name[i] = '_';  // 无效字符替换为下划线
        }
    }

    // 复制扩展名（转换为大写）
    if (ext_len > 0 && dot_pos != NULL) {
        for (size_t i = 0; i < ext_len; i++) {
            char c = dot_pos[1 + i];
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_' || c == '-' || c == '~') {
                short_name[8 + i] = c;
            } else {
                short_name[8 + i] = '_';
            }
        }
    }

    return FAT32_OK;
}

fat32_error_t fat32_dir_convert_from_short_name(const uint8_t *short_name,
                                                char *long_name,
                                                size_t max_len)
{
    avatar_assert(short_name != NULL);
    avatar_assert(long_name != NULL);

    if (max_len < 13) {  // 最大需要12个字符 + 终止符
        return FAT32_ERROR_INVALID_PARAM;
    }

    size_t pos = 0;

    // 复制基本名（去除尾部空格）
    for (int i = 0; i < 8 && pos < max_len - 1; i++) {
        if (short_name[i] != ' ') {
            long_name[pos++] = short_name[i];
        } else {
            break;
        }
    }

    // 检查是否有扩展名
    uint8_t has_ext = 0;
    for (int i = 8; i < 11; i++) {
        if (short_name[i] != ' ') {
            has_ext = 1;
            break;
        }
    }

    // 添加扩展名
    if (has_ext && pos < max_len - 1) {
        long_name[pos++] = '.';
        for (int i = 8; i < 11 && pos < max_len - 1; i++) {
            if (short_name[i] != ' ') {
                long_name[pos++] = short_name[i];
            } else {
                break;
            }
        }
    }

    long_name[pos] = '\0';
    return FAT32_OK;
}

int fat32_dir_compare_short_names(const uint8_t *name1, const uint8_t *name2)
{
    avatar_assert(name1 != NULL);
    avatar_assert(name2 != NULL);

    return memcmp(name1, name2, 11);
}

uint8_t fat32_dir_is_valid_filename(const char *filename)
{
    if (filename == NULL || strlen(filename) == 0 || strlen(filename) > 255) {
        return 0;
    }

    // 检查无效字符（简化实现）
    const char *invalid_chars = "\\/:*?\"<>|";
    for (size_t i = 0; i < strlen(filename); i++) {
        char c = filename[i];
        if (c < 32 || strchr(invalid_chars, c) != NULL) {
            return 0;
        }
    }

    // 检查保留名称（简化实现，只检查几个常见的）
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return 0;
    }

    return 1;
}

/* ============================================================================
 * 私有函数实现
 * ============================================================================ */

static fat32_error_t fat32_dir_read_cluster_data(fat32_disk_t *disk,
                                                 const fat32_fs_info_t *fs_info,
                                                 uint32_t cluster_num,
                                                 uint8_t *buffer)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(buffer != NULL);

    if (!fat32_fat_is_valid_cluster(fs_info, cluster_num)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    // 计算簇对应的扇区
    uint32_t first_sector = fat32_boot_cluster_to_sector(fs_info, cluster_num);

    // 读取整个簇的数据
    return fat32_disk_read_sectors(disk, first_sector, fs_info->sectors_per_cluster, buffer);
}

static fat32_error_t fat32_dir_write_cluster_data(fat32_disk_t *disk,
                                                  const fat32_fs_info_t *fs_info,
                                                  uint32_t cluster_num,
                                                  const uint8_t *buffer)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(buffer != NULL);

    if (!fat32_fat_is_valid_cluster(fs_info, cluster_num)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    // 计算簇对应的扇区
    uint32_t first_sector = fat32_boot_cluster_to_sector(fs_info, cluster_num);

    // 写入整个簇的数据
    return fat32_disk_write_sectors(disk, first_sector, fs_info->sectors_per_cluster, buffer);
}

static fat32_error_t fat32_dir_find_free_entry(fat32_disk_t *disk,
                                               fat32_fs_info_t *fs_info,
                                               uint32_t dir_cluster,
                                               uint32_t *entry_index)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(entry_index != NULL);

    fat32_dir_iterator_t iterator;
    fat32_dir_iterator_init(&iterator, dir_cluster);

    uint32_t current_index = 0;

    while (!iterator.end_of_dir) {
        fat32_dir_entry_t dir_entry;
        fat32_error_t result = fat32_dir_iterator_next(disk, fs_info, &iterator, &dir_entry);
        if (result != FAT32_OK) {
            if (result == FAT32_ERROR_END_OF_FILE) {
                // 到达目录末尾，可以在这里创建新目录项
                *entry_index = current_index;
                return FAT32_OK;
            }
            return result;
        }

        // 检查是否为空闲或已删除的目录项
        if (fat32_dir_is_free_entry(&dir_entry) || fat32_dir_is_deleted_entry(&dir_entry)) {
            *entry_index = current_index;
            return FAT32_OK;
        }

        current_index++;
    }

    // 目录已满，需要扩展目录（简化实现中暂不支持）
    return FAT32_ERROR_NO_SPACE;
}

fat32_error_t fat32_dir_create_directory(fat32_disk_t *disk,
                                         fat32_fs_info_t *fs_info,
                                         uint32_t parent_cluster, /* 父目录的起始簇号 */
                                         const char *dirname,
                                         uint32_t *new_dir_cluster)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(dirname != NULL);
    avatar_assert(new_dir_cluster != NULL);

    // 检查目录名是否有效
    if (!fat32_dir_is_valid_filename(dirname)) {
        return FAT32_ERROR_INVALID_NAME;
    }

    // 检查目录是否已存在
    fat32_dir_entry_t existing_entry;
    fat32_error_t result = fat32_dir_find_entry(disk, fs_info, parent_cluster, dirname,
                                                &existing_entry, NULL);
    if (result == FAT32_OK) {
        return FAT32_ERROR_ALREADY_EXISTS;
    }

    // 分配新目录的簇
    uint32_t dir_cluster;
    result = fat32_fat_allocate_cluster(disk, fs_info, &dir_cluster);
    if (result != FAT32_OK) {
        return result;
    }

    // 清空目录簇并创建标准目录项
    uint8_t *cluster_buffer = (uint8_t *)kalloc_pages((fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    if (cluster_buffer == NULL) {
        fat32_fat_free_cluster(disk, fs_info, dir_cluster);
        return FAT32_ERROR_DISK_ERROR;
    }

    memset(cluster_buffer, 0, fs_info->bytes_per_cluster);

    // 创建 "." 目录项（指向当前目录）
    fat32_dir_entry_t *dot_entry = (fat32_dir_entry_t *)cluster_buffer;
    memset(dot_entry, 0, sizeof(fat32_dir_entry_t));
    memcpy(dot_entry->name, ".          ", 11);  // "." + 10个空格
    dot_entry->attr = FAT32_ATTR_DIRECTORY;
    fat32_dir_set_first_cluster(dot_entry, dir_cluster);
    dot_entry->file_size = 0;
    dot_entry->create_time = 0x0000;
    dot_entry->create_date = 0x0021;
    dot_entry->write_time = 0x0000;
    dot_entry->write_date = 0x0021;
    dot_entry->last_access_date = 0x0021;

    // 创建 ".." 目录项（指向父目录）
    fat32_dir_entry_t *dotdot_entry = (fat32_dir_entry_t *)(cluster_buffer + FAT32_DIR_ENTRY_SIZE);
    memset(dotdot_entry, 0, sizeof(fat32_dir_entry_t));
    memcpy(dotdot_entry->name, "..         ", 11);  // ".." + 9个空格
    dotdot_entry->attr = FAT32_ATTR_DIRECTORY;
    fat32_dir_set_first_cluster(dotdot_entry, parent_cluster);
    dotdot_entry->file_size = 0;
    dotdot_entry->create_time = 0x0000;
    dotdot_entry->create_date = 0x0021;
    dotdot_entry->write_time = 0x0000;
    dotdot_entry->write_date = 0x0021;
    dotdot_entry->last_access_date = 0x0021;

    // 写入目录数据
    result = fat32_dir_write_cluster_data(disk, fs_info, dir_cluster, cluster_buffer);
    if (result != FAT32_OK) {
        kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
        fat32_fat_free_cluster(disk, fs_info, dir_cluster);
        return result;
    }

    kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);

    // 在父目录中创建目录项
    uint32_t entry_index;
    result = fat32_dir_create_entry(disk, fs_info, parent_cluster, dirname,
                                   FAT32_ATTR_DIRECTORY, dir_cluster, 0, &entry_index);
    if (result != FAT32_OK) {
        fat32_fat_free_cluster(disk, fs_info, dir_cluster);
        return result;
    }

    *new_dir_cluster = dir_cluster;
    return FAT32_OK;
}

fat32_error_t fat32_dir_remove_directory(fat32_disk_t *disk,
                                         fat32_fs_info_t *fs_info,
                                         uint32_t parent_cluster,
                                         const char *dirname)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(dirname != NULL);

    // 查找目录
    fat32_dir_entry_t dir_entry;
    uint32_t entry_index;
    fat32_error_t result = fat32_dir_find_entry(disk, fs_info, parent_cluster, dirname,
                                                &dir_entry, &entry_index);
    if (result != FAT32_OK) {
        return result;
    }

    // 检查是否为目录
    if (!fat32_dir_is_directory(&dir_entry)) {
        return FAT32_ERROR_NOT_A_DIRECTORY;
    }

    // 检查目录是否为空
    uint32_t dir_cluster = fat32_dir_get_first_cluster(&dir_entry);
    uint8_t is_empty;
    result = fat32_dir_is_empty(disk, fs_info, dir_cluster, &is_empty);
    if (result != FAT32_OK) {
        return result;
    }

    if (!is_empty) {
        return FAT32_ERROR_DIRECTORY_NOT_EMPTY;
    }

    // 释放目录占用的簇
    if (dir_cluster >= 2) {
        result = fat32_fat_free_cluster_chain(disk, fs_info, dir_cluster);
        if (result != FAT32_OK) {
            return result;
        }
    }

    // 删除目录项
    return fat32_dir_delete_entry(disk, fs_info, parent_cluster, entry_index);
}
