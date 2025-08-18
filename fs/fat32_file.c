/**
 * @file fat32_file.c
 * @brief FAT32文件操作实现
 * 
 * 本文件实现了FAT32文件系统的文件操作功能。
 */

#include "fs/fat32.h"
#include "fs/fat32_file.h"
#include "fs/fat32_fat.h"
#include "fs/fat32_dir.h"
#include "fs/fat32_boot.h"
#include "lib/avatar_string.h"
#include "lib/avatar_assert.h"
#include "mem/mem.h"
#include "io.h"

/* ============================================================================
 * 全局变量
 * ============================================================================ */

static fat32_file_handle_t g_file_handles[FAT32_MAX_OPEN_FILES];
static uint8_t g_file_handle_initialized = 0;

/* ============================================================================
 * 私有函数声明
 * ============================================================================ */

static fat32_error_t fat32_file_parse_path(const char *filepath, 
                                           char *dirname, 
                                           char *filename);

static fat32_error_t fat32_file_find_directory(fat32_disk_t *disk,
                                               const fat32_fs_info_t *fs_info,
                                               const char *dirpath,
                                               uint32_t *dir_cluster);

static fat32_error_t fat32_file_read_cluster_data(fat32_disk_t *disk,
                                                  const fat32_fs_info_t *fs_info,
                                                  uint32_t cluster_num,
                                                  uint32_t offset,
                                                  void *buffer,
                                                  uint32_t size,
                                                  uint32_t *bytes_read);

static fat32_error_t fat32_file_write_cluster_data(fat32_disk_t *disk,
                                                   fat32_fs_info_t *fs_info,
                                                   uint32_t cluster_num,
                                                   uint32_t offset,
                                                   const void *buffer,
                                                   uint32_t size,
                                                   uint32_t *bytes_written);

static fat32_error_t fat32_file_update_dir_entry(fat32_disk_t *disk,
                                                 fat32_fs_info_t *fs_info,
                                                 const char *filepath,
                                                 uint32_t new_size,
                                                 uint32_t first_cluster);

static fat32_error_t fat32_file_extend_if_needed(fat32_disk_t *disk,
                                                 fat32_fs_info_t *fs_info,
                                                 fat32_file_handle_t *file_handle,
                                                 uint32_t required_size);

/* ============================================================================
 * 文件句柄管理函数实现
 * ============================================================================ */

fat32_error_t fat32_file_handle_init(void)
{
    if (g_file_handle_initialized) {
        return FAT32_OK;
    }
    
    // 初始化所有文件句柄
    for (int i = 0; i < FAT32_MAX_OPEN_FILES; i++) {
        memset(&g_file_handles[i], 0, sizeof(fat32_file_handle_t));
        g_file_handles[i].in_use = 0;
    }
    
    g_file_handle_initialized = 1;
    logger("FAT32: File handle manager initialized\n");
    
    return FAT32_OK;
}

fat32_error_t fat32_file_handle_alloc(fat32_file_handle_t **file_handle)
{
    avatar_assert(file_handle != NULL);
    
    if (!g_file_handle_initialized) {
        fat32_error_t result = fat32_file_handle_init();
        if (result != FAT32_OK) {
            return result;
        }
    }
    
    // 查找空闲的文件句柄
    for (int i = 0; i < FAT32_MAX_OPEN_FILES; i++) {
        if (!g_file_handles[i].in_use) {
            memset(&g_file_handles[i], 0, sizeof(fat32_file_handle_t));
            g_file_handles[i].in_use = 1;
            *file_handle = &g_file_handles[i];
            return FAT32_OK;
        }
    }
    
    return FAT32_ERROR_TOO_MANY_OPEN_FILES;
}

fat32_error_t fat32_file_handle_free(fat32_file_handle_t *file_handle)
{
    avatar_assert(file_handle != NULL);
    
    if (!fat32_file_is_valid_handle(file_handle)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    memset(file_handle, 0, sizeof(fat32_file_handle_t));
    file_handle->in_use = 0;
    
    return FAT32_OK;
}

/* ============================================================================
 * 文件操作函数实现
 * ============================================================================ */

fat32_error_t fat32_file_open(fat32_disk_t *disk,
                              fat32_fs_info_t *fs_info,
                              const char *filepath,
                              uint32_t flags,
                              fat32_file_handle_t **file_handle)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(filepath != NULL);
    avatar_assert(file_handle != NULL);
    
    // 分配文件句柄
    fat32_error_t result = fat32_file_handle_alloc(file_handle);
    if (result != FAT32_OK) {
        return result;
    }
    
    fat32_file_handle_t *handle = *file_handle;
    
    // 解析文件路径
    char dirname[FAT32_MAX_PATH];
    char filename[FAT32_MAX_FILENAME];
    result = fat32_file_parse_path(filepath, dirname, filename);
    if (result != FAT32_OK) {
        fat32_file_handle_free(handle);
        return result;
    }
    
    // 查找目录
    uint32_t dir_cluster;
    result = fat32_file_find_directory(disk, fs_info, dirname, &dir_cluster);
    if (result != FAT32_OK) {
        fat32_file_handle_free(handle);
        return result;
    }
    
    // 查找文件
    fat32_dir_entry_t dir_entry;
    uint32_t entry_index;
    result = fat32_dir_find_entry(disk, fs_info, dir_cluster, filename, &dir_entry, &entry_index);
    
    if (result == FAT32_OK) {
        // 文件存在
        if (fat32_dir_is_directory(&dir_entry)) {
            fat32_file_handle_free(handle);
            return FAT32_ERROR_NOT_A_FILE;
        }
        
        // 检查截断标志
        if (flags & FAT32_O_TRUNC) {
            // 截断文件到0字节
            if (dir_entry.file_size > 0) {
                uint32_t first_cluster = fat32_dir_get_first_cluster(&dir_entry);
                if (first_cluster >= 2) {
                    fat32_fat_free_cluster_chain(disk, fs_info, first_cluster);
                }
                dir_entry.file_size = 0;
                fat32_dir_set_first_cluster(&dir_entry, 0);
                fat32_dir_write_entry(disk, fs_info, dir_cluster, entry_index, &dir_entry);
            }
        }
        
    } else if (result == FAT32_ERROR_NOT_FOUND) {
        // 文件不存在
        if (flags & FAT32_O_CREAT) {
            // 创建新文件
            result = fat32_dir_create_entry(disk, fs_info, dir_cluster, filename, 
                                           FAT32_ATTR_ARCHIVE, 0, 0, &entry_index);
            if (result != FAT32_OK) {
                fat32_file_handle_free(handle);
                return result;
            }
            
            // 重新读取目录项
            result = fat32_dir_read_entry(disk, fs_info, dir_cluster, entry_index, &dir_entry);
            if (result != FAT32_OK) {
                fat32_file_handle_free(handle);
                return result;
            }
        } else {
            fat32_file_handle_free(handle);
            return FAT32_ERROR_NOT_FOUND;
        }
    } else {
        fat32_file_handle_free(handle);
        return result;
    }
    
    // 初始化文件句柄
    handle->first_cluster = fat32_dir_get_first_cluster(&dir_entry);
    handle->current_cluster = handle->first_cluster;
    handle->cluster_offset = 0;
    handle->file_size = dir_entry.file_size;
    handle->file_position = 0;
    handle->attr = dir_entry.attr;
    handle->flags = flags;
    handle->modified = 0;
    handle->dir_cluster = dir_cluster;
    handle->dir_entry_index = entry_index;
    
    // 设置追加模式
    if (flags & FAT32_O_APPEND) {
        handle->file_position = handle->file_size;
        // 定位到文件末尾的簇
        if (handle->file_size > 0 && handle->first_cluster >= 2) {
            uint32_t cluster_index = handle->file_size / fs_info->bytes_per_cluster;
            result = fat32_fat_get_cluster_at_index(disk, fs_info, handle->first_cluster, 
                                                   cluster_index, &handle->current_cluster);
            if (result != FAT32_OK) {
                handle->current_cluster = handle->first_cluster;
            }
            handle->cluster_offset = handle->file_size % fs_info->bytes_per_cluster;
        }
    }
    
    // 复制文件名用于调试
    strncpy(handle->filename, filename, FAT32_MAX_FILENAME - 1);
    handle->filename[FAT32_MAX_FILENAME - 1] = '\0';
    
    logger("FAT32: File '%s' opened successfully, size=%u, cluster=%u\n", 
           filename, handle->file_size, handle->first_cluster);
    
    return FAT32_OK;
}

fat32_error_t fat32_file_close(fat32_disk_t *disk,
                               fat32_fs_info_t *fs_info,
                               fat32_file_handle_t *file_handle)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(file_handle != NULL);

    if (!fat32_file_is_valid_handle(file_handle)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    // 如果文件被修改过，更新目录项
    if (file_handle->modified && fat32_file_is_writable(file_handle)) {
        // 直接更新目录项，使用保存的目录信息
        fat32_dir_entry_t dir_entry;
        fat32_error_t result = fat32_dir_read_entry(disk, fs_info, file_handle->dir_cluster,
                                                   file_handle->dir_entry_index, &dir_entry);
        if (result == FAT32_OK) {
            // 更新文件大小和起始簇
            dir_entry.file_size = file_handle->file_size;
            fat32_dir_set_first_cluster(&dir_entry, file_handle->first_cluster);

            // 更新修改时间（简化实现）
            dir_entry.write_time = 0x0000;
            dir_entry.write_date = 0x0021;

            result = fat32_dir_write_entry(disk, fs_info, file_handle->dir_cluster,
                                          file_handle->dir_entry_index, &dir_entry);
            if (result != FAT32_OK) {
                logger("FAT32: Warning - Failed to update directory entry for '%s': %s\n",
                       file_handle->filename, fat32_get_error_string(result));
            }
        }
    }

    // 刷新缓存数据
    fat32_error_t result = fat32_file_flush(disk, fs_info, file_handle);
    if (result != FAT32_OK) {
        logger("FAT32: Warning - Failed to flush file '%s'\n", file_handle->filename);
    }

    logger("FAT32: File '%s' closed (size: %u bytes)\n", file_handle->filename, file_handle->file_size);

    // 释放文件句柄
    return fat32_file_handle_free(file_handle);
}

uint32_t fat32_file_tell(const fat32_file_handle_t *file_handle)
{
    avatar_assert(file_handle != NULL);
    
    if (!fat32_file_is_valid_handle(file_handle)) {
        return 0;
    }
    
    return file_handle->file_position;
}

fat32_error_t fat32_file_flush(fat32_disk_t *disk,
                               fat32_fs_info_t *fs_info,
                               fat32_file_handle_t *file_handle)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(file_handle != NULL);
    
    if (!fat32_file_is_valid_handle(file_handle)) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 简化实现中没有缓存，直接返回成功
    // 实际实现中需要将缓存的数据写入磁盘
    
    return FAT32_OK;
}

fat32_error_t fat32_file_read(fat32_disk_t *disk,
                              const fat32_fs_info_t *fs_info,
                              fat32_file_handle_t *file_handle,
                              void *buffer,
                              uint32_t size,
                              uint32_t *bytes_read)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(file_handle != NULL);
    avatar_assert(buffer != NULL);
    avatar_assert(bytes_read != NULL);

    *bytes_read = 0;

    if (!fat32_file_is_valid_handle(file_handle)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    if (!fat32_file_is_readable(file_handle)) {
        return FAT32_ERROR_ACCESS_DENIED;
    }

    if (fat32_file_is_eof(file_handle) || size == 0) {
        return FAT32_OK;
    }

    // 计算实际可读取的字节数
    uint32_t remaining_in_file = file_handle->file_size - file_handle->file_position;
    uint32_t bytes_to_read = (size < remaining_in_file) ? size : remaining_in_file;

    if (bytes_to_read == 0) {
        return FAT32_OK;
    }

    uint8_t *read_buffer = (uint8_t *)buffer;
    uint32_t total_read = 0;

    // 如果文件没有分配簇，直接返回
    if (file_handle->first_cluster < 2) {
        return FAT32_OK;
    }

    uint32_t current_cluster = file_handle->current_cluster;
    uint32_t cluster_offset = file_handle->cluster_offset;

    while (total_read < bytes_to_read && current_cluster >= 2) {
        uint32_t bytes_in_cluster = bytes_to_read - total_read;
        uint32_t remaining_in_cluster = fs_info->bytes_per_cluster - cluster_offset;

        if (bytes_in_cluster > remaining_in_cluster) {
            bytes_in_cluster = remaining_in_cluster;
        }

        uint32_t cluster_bytes_read;
        fat32_error_t result = fat32_file_read_cluster_data(disk, fs_info, current_cluster,
                                                           cluster_offset, read_buffer + total_read,
                                                           bytes_in_cluster, &cluster_bytes_read);
        if (result != FAT32_OK) {
            return result;
        }

        total_read += cluster_bytes_read;
        cluster_offset += cluster_bytes_read;

        // 如果读完了当前簇，移动到下一个簇
        if (cluster_offset >= fs_info->bytes_per_cluster) {
            uint32_t next_cluster;
            result = fat32_fat_get_next_cluster(disk, fs_info, current_cluster, &next_cluster);
            if (result != FAT32_OK) {
                return result;
            }

            if (next_cluster == 0) {
                break;  // 簇链结束
            }

            current_cluster = next_cluster;
            cluster_offset = 0;
        }

        if (cluster_bytes_read < bytes_in_cluster) {
            break;  // 读取不完整，可能到达文件末尾
        }
    }

    // 更新文件句柄状态
    file_handle->file_position += total_read;
    file_handle->current_cluster = current_cluster;
    file_handle->cluster_offset = cluster_offset;

    *bytes_read = total_read;
    return FAT32_OK;
}

fat32_error_t fat32_file_seek(fat32_file_handle_t *file_handle,
                              int32_t offset,
                              int whence,
                              uint32_t *new_position)
{
    avatar_assert(file_handle != NULL);

    if (!fat32_file_is_valid_handle(file_handle)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    uint32_t target_position;

    switch (whence) {
        case FAT32_SEEK_SET:
            if (offset < 0) {
                return FAT32_ERROR_INVALID_PARAM;
            }
            target_position = (uint32_t)offset;
            break;

        case FAT32_SEEK_CUR:
            if (offset < 0 && (uint32_t)(-offset) > file_handle->file_position) {
                return FAT32_ERROR_INVALID_PARAM;
            }
            target_position = file_handle->file_position + offset;
            break;

        case FAT32_SEEK_END:
            if (offset < 0 && (uint32_t)(-offset) > file_handle->file_size) {
                return FAT32_ERROR_INVALID_PARAM;
            }
            target_position = file_handle->file_size + offset;
            break;

        default:
            return FAT32_ERROR_INVALID_PARAM;
    }

    // 简化实现：只允许在文件大小范围内定位
    if (target_position > file_handle->file_size) {
        target_position = file_handle->file_size;
    }

    // 更新文件位置
    file_handle->file_position = target_position;

    // 重新计算当前簇和偏移（简化实现）
    if (target_position == 0 || file_handle->first_cluster < 2) {
        file_handle->current_cluster = file_handle->first_cluster;
        file_handle->cluster_offset = 0;
    } else {
        // 简化实现：重置到文件开头，实际应该优化定位算法
        file_handle->current_cluster = file_handle->first_cluster;
        file_handle->cluster_offset = target_position % 4096; // 使用固定的簇大小
    }

    if (new_position != NULL) {
        *new_position = target_position;
    }

    return FAT32_OK;
}

/* ============================================================================
 * 私有函数实现
 * ============================================================================ */

static fat32_error_t fat32_file_parse_path(const char *filepath,
                                           char *dirname,
                                           char *filename)
{
    avatar_assert(filepath != NULL);
    avatar_assert(dirname != NULL);
    avatar_assert(filename != NULL);

    size_t path_len = strlen(filepath);
    if (path_len == 0 || path_len >= FAT32_MAX_PATH) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    // 查找最后一个路径分隔符
    const char *last_slash = strrchr(filepath, '/');

    if (last_slash == NULL) {
        // 没有路径分隔符，文件在根目录
        strcpy(dirname, "/");
        strcpy(filename, filepath);
    } else if (last_slash == filepath) {
        // 路径分隔符在开头，文件在根目录
        strcpy(dirname, "/");
        strcpy(filename, last_slash + 1);
    } else {
        // 分离目录和文件名
        size_t dir_len = last_slash - filepath;
        strncpy(dirname, filepath, dir_len);
        dirname[dir_len] = '\0';
        strcpy(filename, last_slash + 1);
    }

    // 检查文件名是否有效
    if (strlen(filename) == 0) {
        return FAT32_ERROR_INVALID_NAME;
    }

    return FAT32_OK;
}

static fat32_error_t fat32_file_find_directory(fat32_disk_t *disk,
                                               const fat32_fs_info_t *fs_info,
                                               const char *dirpath,
                                               uint32_t *dir_cluster)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(dirpath != NULL);
    avatar_assert(dir_cluster != NULL);

    // 简化实现：只支持根目录
    if (strcmp(dirpath, "/") == 0) {
        *dir_cluster = fs_info->boot_sector.root_cluster;
        return FAT32_OK;
    }

    // 复杂路径解析暂不实现
    return FAT32_ERROR_NOT_FOUND;
}

static fat32_error_t fat32_file_read_cluster_data(fat32_disk_t *disk,
                                                  const fat32_fs_info_t *fs_info,
                                                  uint32_t cluster_num,
                                                  uint32_t offset,
                                                  void *buffer,
                                                  uint32_t size,
                                                  uint32_t *bytes_read)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(buffer != NULL);
    avatar_assert(bytes_read != NULL);

    *bytes_read = 0;

    if (!fat32_fat_is_valid_cluster(fs_info, cluster_num)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    if (offset >= fs_info->bytes_per_cluster) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    // 计算实际可读取的字节数
    uint32_t remaining_in_cluster = fs_info->bytes_per_cluster - offset;
    uint32_t bytes_to_read = (size < remaining_in_cluster) ? size : remaining_in_cluster;

    if (bytes_to_read == 0) {
        return FAT32_OK;
    }

    // 分配簇缓冲区
    uint8_t *cluster_buffer = (uint8_t *)kalloc_pages((fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    if (cluster_buffer == NULL) {
        return FAT32_ERROR_DISK_ERROR;
    }

    // 计算簇对应的扇区
    uint32_t first_sector = fat32_boot_cluster_to_sector(fs_info, cluster_num);

    // 读取整个簇
    fat32_error_t result = fat32_disk_read_sectors(disk, first_sector, fs_info->sectors_per_cluster, cluster_buffer);
    if (result != FAT32_OK) {
        kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
        return result;
    }

    // 复制所需的数据
    memcpy(buffer, cluster_buffer + offset, bytes_to_read);
    *bytes_read = bytes_to_read;

    kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    return FAT32_OK;
}

static fat32_error_t fat32_file_write_cluster_data(fat32_disk_t *disk,
                                                   fat32_fs_info_t *fs_info,
                                                   uint32_t cluster_num,
                                                   uint32_t offset,
                                                   const void *buffer,
                                                   uint32_t size,
                                                   uint32_t *bytes_written)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(buffer != NULL);
    avatar_assert(bytes_written != NULL);

    *bytes_written = 0;

    if (!fat32_fat_is_valid_cluster(fs_info, cluster_num)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    if (offset >= fs_info->bytes_per_cluster) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    // 计算实际可写入的字节数
    uint32_t remaining_in_cluster = fs_info->bytes_per_cluster - offset;
    uint32_t bytes_to_write = (size < remaining_in_cluster) ? size : remaining_in_cluster;

    if (bytes_to_write == 0) {
        return FAT32_OK;
    }

    // 分配簇缓冲区
    uint8_t *cluster_buffer = (uint8_t *)kalloc_pages((fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    if (cluster_buffer == NULL) {
        return FAT32_ERROR_DISK_ERROR;
    }

    // 计算簇对应的扇区
    uint32_t first_sector = fat32_boot_cluster_to_sector(fs_info, cluster_num);

    fat32_error_t result;

    // 如果不是写整个簇，需要先读取原有数据
    if (offset != 0 || bytes_to_write != fs_info->bytes_per_cluster) {
        result = fat32_disk_read_sectors(disk, first_sector, fs_info->sectors_per_cluster, cluster_buffer);
        if (result != FAT32_OK) {
            kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
            return result;
        }
    }

    // 复制新数据
    memcpy(cluster_buffer + offset, buffer, bytes_to_write);

    // 写回整个簇
    result = fat32_disk_write_sectors(disk, first_sector, fs_info->sectors_per_cluster, cluster_buffer);
    if (result != FAT32_OK) {
        kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
        return result;
    }

    *bytes_written = bytes_to_write;

    kfree_pages(cluster_buffer, (fs_info->bytes_per_cluster + PAGE_SIZE - 1) / PAGE_SIZE);
    return FAT32_OK;
}

/* ============================================================================
 * 文件管理函数实现
 * ============================================================================ */

fat32_error_t fat32_file_create(fat32_disk_t *disk,
                                fat32_fs_info_t *fs_info,
                                const char *filepath,
                                uint8_t attr)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(filepath != NULL);

    // 解析文件路径
    char dirname[FAT32_MAX_PATH];
    char filename[FAT32_MAX_FILENAME];
    fat32_error_t result = fat32_file_parse_path(filepath, dirname, filename);
    if (result != FAT32_OK) {
        return result;
    }

    // 查找目录
    uint32_t dir_cluster;
    result = fat32_file_find_directory(disk, fs_info, dirname, &dir_cluster);
    if (result != FAT32_OK) {
        return result;
    }

    // 创建文件目录项
    uint32_t entry_index;
    return fat32_dir_create_entry(disk, fs_info, dir_cluster, filename,
                                 attr, 0, 0, &entry_index);
}

fat32_error_t fat32_file_delete(fat32_disk_t *disk,
                                fat32_fs_info_t *fs_info,
                                const char *filepath)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(filepath != NULL);

    // 解析文件路径
    char dirname[FAT32_MAX_PATH];
    char filename[FAT32_MAX_FILENAME];
    fat32_error_t result = fat32_file_parse_path(filepath, dirname, filename);
    if (result != FAT32_OK) {
        return result;
    }

    // 查找目录
    uint32_t dir_cluster;
    result = fat32_file_find_directory(disk, fs_info, dirname, &dir_cluster);
    if (result != FAT32_OK) {
        return result;
    }

    // 查找文件
    fat32_dir_entry_t dir_entry;
    uint32_t entry_index;
    result = fat32_dir_find_entry(disk, fs_info, dir_cluster, filename, &dir_entry, &entry_index);
    if (result != FAT32_OK) {
        return result;
    }

    // 检查是否为文件
    if (fat32_dir_is_directory(&dir_entry)) {
        return FAT32_ERROR_NOT_A_FILE;
    }

    // 释放文件占用的簇
    uint32_t first_cluster = fat32_dir_get_first_cluster(&dir_entry);
    if (first_cluster >= 2) {
        result = fat32_fat_free_cluster_chain(disk, fs_info, first_cluster);
        if (result != FAT32_OK) {
            return result;
        }
    }

    // 删除目录项
    return fat32_dir_delete_entry(disk, fs_info, dir_cluster, entry_index);
}

fat32_error_t fat32_file_rename(fat32_disk_t *disk,
                                fat32_fs_info_t *fs_info,
                                const char *old_path,
                                const char *new_path)
{
    // 简化实现：暂不支持重命名
    return FAT32_ERROR_ACCESS_DENIED;
}

fat32_error_t fat32_file_stat(fat32_disk_t *disk,
                              const fat32_fs_info_t *fs_info,
                              const char *filepath,
                              fat32_dir_entry_t *dir_entry)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(filepath != NULL);
    avatar_assert(dir_entry != NULL);

    // 解析文件路径
    char dirname[FAT32_MAX_PATH];
    char filename[FAT32_MAX_FILENAME];
    fat32_error_t result = fat32_file_parse_path(filepath, dirname, filename);
    if (result != FAT32_OK) {
        return result;
    }

    // 查找目录
    uint32_t dir_cluster;
    result = fat32_file_find_directory(disk, fs_info, dirname, &dir_cluster);
    if (result != FAT32_OK) {
        return result;
    }

    // 查找文件
    return fat32_dir_find_entry(disk, fs_info, dir_cluster, filename, dir_entry, NULL);
}

fat32_error_t fat32_file_write(fat32_disk_t *disk,
                               fat32_fs_info_t *fs_info,
                               fat32_file_handle_t *file_handle,
                               const void *buffer,
                               uint32_t size,
                               uint32_t *bytes_written)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(file_handle != NULL);
    avatar_assert(buffer != NULL);
    avatar_assert(bytes_written != NULL);

    *bytes_written = 0;

    if (!fat32_file_is_valid_handle(file_handle)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    if (!fat32_file_is_writable(file_handle)) {
        return FAT32_ERROR_ACCESS_DENIED;
    }

    if (size == 0) {
        return FAT32_OK;
    }

    // 计算写入后的文件大小
    uint32_t end_position = file_handle->file_position + size;

    // 如果需要扩展文件，先分配足够的簇
    if (end_position > file_handle->file_size) {
        fat32_error_t result = fat32_file_extend_if_needed(disk, fs_info, file_handle, end_position);
        if (result != FAT32_OK) {
            return result;
        }
    }

    const uint8_t *write_buffer = (const uint8_t *)buffer;
    uint32_t total_written = 0;
    uint32_t current_cluster = file_handle->current_cluster;
    uint32_t cluster_offset = file_handle->cluster_offset;

    // 如果文件为空或当前簇无效，需要分配第一个簇
    if (current_cluster < 2) {
        if (file_handle->first_cluster < 2) {
            // 分配第一个簇
            fat32_error_t result = fat32_fat_allocate_cluster(disk, fs_info, &current_cluster);
            if (result != FAT32_OK) {
                return result;
            }
            file_handle->first_cluster = current_cluster;
            file_handle->current_cluster = current_cluster;
        } else {
            current_cluster = file_handle->first_cluster;
            file_handle->current_cluster = current_cluster;
        }
        cluster_offset = 0;
        file_handle->cluster_offset = 0;
    }

    while (total_written < size && current_cluster >= 2) {
        uint32_t bytes_in_cluster = size - total_written;
        uint32_t remaining_in_cluster = fs_info->bytes_per_cluster - cluster_offset;

        if (bytes_in_cluster > remaining_in_cluster) {
            bytes_in_cluster = remaining_in_cluster;
        }

        uint32_t cluster_bytes_written;
        fat32_error_t result = fat32_file_write_cluster_data(disk, fs_info, current_cluster,
                                                            cluster_offset, write_buffer + total_written,
                                                            bytes_in_cluster, &cluster_bytes_written);
        if (result != FAT32_OK) {
            break;
        }

        total_written += cluster_bytes_written;
        cluster_offset += cluster_bytes_written;

        // 如果写满了当前簇，移动到下一个簇
        if (cluster_offset >= fs_info->bytes_per_cluster) {
            uint32_t next_cluster;
            result = fat32_fat_get_next_cluster(disk, fs_info, current_cluster, &next_cluster);
            if (result != FAT32_OK) {
                break;
            }

            if (next_cluster == 0) {
                // 需要分配新簇
                result = fat32_fat_extend_cluster_chain(disk, fs_info, current_cluster, &next_cluster);
                if (result != FAT32_OK) {
                    break;
                }
            }

            current_cluster = next_cluster;
            cluster_offset = 0;
        }

        if (cluster_bytes_written < bytes_in_cluster) {
            break;  // 写入不完整
        }
    }

    // 更新文件句柄状态
    file_handle->file_position += total_written;
    file_handle->current_cluster = current_cluster;
    file_handle->cluster_offset = cluster_offset;

    // 更新文件大小
    if (file_handle->file_position > file_handle->file_size) {
        file_handle->file_size = file_handle->file_position;
    }

    // 标记文件为已修改
    if (total_written > 0) {
        file_handle->modified = 1;
    }

    *bytes_written = total_written;
    return FAT32_OK;
}

fat32_error_t fat32_file_truncate(fat32_disk_t *disk,
                                  fat32_fs_info_t *fs_info,
                                  fat32_file_handle_t *file_handle,
                                  uint32_t new_size)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(file_handle != NULL);

    if (!fat32_file_is_valid_handle(file_handle)) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    if (!fat32_file_is_writable(file_handle)) {
        return FAT32_ERROR_ACCESS_DENIED;
    }

    if (new_size == file_handle->file_size) {
        return FAT32_OK;  // 无需改变
    }

    if (new_size > file_handle->file_size) {
        // 扩展文件 - 确保有足够的簇
        return fat32_file_extend_if_needed(disk, fs_info, file_handle, new_size);
    }

    // 缩小文件
    if (new_size == 0) {
        // 截断为空文件，释放所有簇
        if (file_handle->first_cluster >= 2) {
            fat32_error_t result = fat32_fat_free_cluster_chain(disk, fs_info, file_handle->first_cluster);
            if (result != FAT32_OK) {
                return result;
            }
            file_handle->first_cluster = 0;
        }
        file_handle->current_cluster = 0;
        file_handle->cluster_offset = 0;
    } else {
        // 计算新文件大小需要的簇数
        uint32_t clusters_needed = (new_size + fs_info->bytes_per_cluster - 1) / fs_info->bytes_per_cluster;

        // 找到第clusters_needed个簇
        uint32_t current_cluster = file_handle->first_cluster;
        uint32_t cluster_index = 0;

        while (current_cluster >= 2 && cluster_index < clusters_needed - 1) {
            uint32_t next_cluster;
            fat32_error_t result = fat32_fat_get_next_cluster(disk, fs_info, current_cluster, &next_cluster);
            if (result != FAT32_OK) {
                return result;
            }

            if (next_cluster == 0) {
                break;  // 簇链结束
            }

            current_cluster = next_cluster;
            cluster_index++;
        }

        if (current_cluster >= 2) {
            // 获取要释放的簇链
            uint32_t next_cluster;
            fat32_error_t result = fat32_fat_get_next_cluster(disk, fs_info, current_cluster, &next_cluster);
            if (result != FAT32_OK) {
                return result;
            }

            // 将当前簇标记为簇链结束
            result = fat32_fat_write_entry(disk, fs_info, current_cluster, FAT32_EOC_MAX);
            if (result != FAT32_OK) {
                return result;
            }

            // 释放后续簇
            if (next_cluster >= 2) {
                result = fat32_fat_free_cluster_chain(disk, fs_info, next_cluster);
                if (result != FAT32_OK) {
                    return result;
                }
            }
        }
    }

    // 更新文件大小
    file_handle->file_size = new_size;
    file_handle->modified = 1;  // 标记为已修改

    // 调整文件位置
    if (file_handle->file_position > new_size) {
        file_handle->file_position = new_size;

        // 重新计算当前簇和偏移
        if (new_size == 0) {
            file_handle->current_cluster = file_handle->first_cluster;
            file_handle->cluster_offset = 0;
        } else {
            uint32_t cluster_index = file_handle->file_position / fs_info->bytes_per_cluster;
            fat32_error_t result = fat32_fat_get_cluster_at_index(disk, fs_info,
                                                                 file_handle->first_cluster,
                                                                 cluster_index,
                                                                 &file_handle->current_cluster);
            if (result == FAT32_OK) {
                file_handle->cluster_offset = file_handle->file_position % fs_info->bytes_per_cluster;
            }
        }
    }

    return FAT32_OK;
}

static fat32_error_t fat32_file_extend_if_needed(fat32_disk_t *disk,
                                                 fat32_fs_info_t *fs_info,
                                                 fat32_file_handle_t *file_handle,
                                                 uint32_t required_size)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(file_handle != NULL);

    if (required_size <= file_handle->file_size) {
        return FAT32_OK;  // 无需扩展
    }

    // 计算当前文件占用的簇数
    uint32_t current_clusters = 0;
    if (file_handle->file_size > 0) {
        current_clusters = (file_handle->file_size + fs_info->bytes_per_cluster - 1) / fs_info->bytes_per_cluster;
    }

    // 计算需要的簇数
    uint32_t required_clusters = (required_size + fs_info->bytes_per_cluster - 1) / fs_info->bytes_per_cluster;

    if (required_clusters <= current_clusters) {
        return FAT32_OK;  // 当前簇数足够
    }

    uint32_t clusters_to_add = required_clusters - current_clusters;

    if (file_handle->first_cluster < 2) {
        // 文件还没有分配簇，分配第一个簇链
        return fat32_fat_allocate_cluster_chain(disk, fs_info, required_clusters, &file_handle->first_cluster);
    } else {
        // 找到簇链的最后一个簇
        uint32_t last_cluster = file_handle->first_cluster;
        uint32_t next_cluster;

        while (1) {
            fat32_error_t result = fat32_fat_get_next_cluster(disk, fs_info, last_cluster, &next_cluster);
            if (result != FAT32_OK) {
                return result;
            }

            if (next_cluster == 0) {
                break;  // 找到最后一个簇
            }

            last_cluster = next_cluster;
        }

        // 在簇链末尾添加新簇
        for (uint32_t i = 0; i < clusters_to_add; i++) {
            uint32_t new_cluster;
            fat32_error_t result = fat32_fat_extend_cluster_chain(disk, fs_info, last_cluster, &new_cluster);
            if (result != FAT32_OK) {
                return result;
            }
            last_cluster = new_cluster;
        }
    }

    return FAT32_OK;
}

static fat32_error_t fat32_file_update_dir_entry(fat32_disk_t *disk,
                                                 fat32_fs_info_t *fs_info,
                                                 const char *filepath,
                                                 uint32_t new_size,
                                                 uint32_t first_cluster)
{
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(filepath != NULL);

    // 解析文件路径
    char dirname[FAT32_MAX_PATH];
    char filename[FAT32_MAX_FILENAME];
    fat32_error_t result = fat32_file_parse_path(filepath, dirname, filename);
    if (result != FAT32_OK) {
        return result;
    }

    // 查找目录
    uint32_t dir_cluster;
    result = fat32_file_find_directory(disk, fs_info, dirname, &dir_cluster);
    if (result != FAT32_OK) {
        return result;
    }

    // 查找文件的目录项
    fat32_dir_entry_t dir_entry;
    uint32_t entry_index;
    result = fat32_dir_find_entry(disk, fs_info, dir_cluster, filename, &dir_entry, &entry_index);
    if (result != FAT32_OK) {
        return result;
    }

    // 更新目录项
    dir_entry.file_size = new_size;
    fat32_dir_set_first_cluster(&dir_entry, first_cluster);

    // 更新修改时间（简化实现，使用固定值）
    dir_entry.write_time = 0x0000;
    dir_entry.write_date = 0x0021;

    // 写回目录项
    return fat32_dir_write_entry(disk, fs_info, dir_cluster, entry_index, &dir_entry);
}
