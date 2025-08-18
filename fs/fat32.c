/**
 * @file fat32.c
 * @brief FAT32文件系统主接口实现
 * 
 * 本文件实现了FAT32文件系统的统一对外接口。
 */

#include "fs/fat32.h"
#include "lib/avatar_string.h"
#include "lib/avatar_assert.h"
#include "io.h"

/* ============================================================================
 * 全局变量
 * ============================================================================ */

fat32_context_t g_fat32_context;

/* ============================================================================
 * 文件系统管理函数实现
 * ============================================================================ */

fat32_error_t fat32_init(void)
{
    if (g_fat32_context.initialized) {
        return FAT32_OK;
    }
    
    // 清零上下文
    memset(&g_fat32_context, 0, sizeof(fat32_context_t));
    
    // 初始化虚拟磁盘
    g_fat32_context.disk = fat32_get_disk();
    fat32_error_t result = fat32_disk_init(g_fat32_context.disk);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to initialize disk\n");
        return result;
    }
    
    // 初始化缓存管理器
    result = fat32_init_global_cache();
    if (result != FAT32_OK) {
        logger("FAT32: Failed to initialize cache\n");
        fat32_disk_cleanup(g_fat32_context.disk);
        return result;
    }
    g_fat32_context.cache_mgr = fat32_get_cache_manager();
    
    // 初始化文件句柄管理器
    result = fat32_file_handle_init();
    if (result != FAT32_OK) {
        logger("FAT32: Failed to initialize file handle manager\n");
        fat32_cleanup_global_cache(g_fat32_context.disk, &g_fat32_context.fs_info);
        fat32_disk_cleanup(g_fat32_context.disk);
        return result;
    }
    
    g_fat32_context.initialized = 1;
    g_fat32_context.mounted = 0;
    
    logger("FAT32: File system initialized successfully\n");
    return FAT32_OK;
}

fat32_error_t fat32_format_and_mount(const char *volume_label)
{
    if (!g_fat32_context.initialized) {
        fat32_error_t result = fat32_init();
        if (result != FAT32_OK) {
            return result;
        }
    }
    
    if (g_fat32_context.mounted) {
        logger("FAT32: File system already mounted\n");
        return FAT32_ERROR_ALREADY_EXISTS;
    }
    
    // 格式化磁盘
    fat32_error_t result = fat32_disk_format(g_fat32_context.disk, volume_label);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to format disk\n");
        return result;
    }
    
    // 挂载文件系统
    result = fat32_mount();
    if (result != FAT32_OK) {
        logger("FAT32: Failed to mount after format\n");
        return result;
    }
    
    logger("FAT32: File system formatted and mounted successfully\n");
    return FAT32_OK;
}

fat32_error_t fat32_mount(void)
{
    if (!g_fat32_context.initialized) {
        return FAT32_ERROR_NOT_MOUNTED;
    }
    
    if (g_fat32_context.mounted) {
        return FAT32_OK;
    }
    
    // 读取引导扇区和文件系统信息
    fat32_error_t result = fat32_boot_read_boot_sector(g_fat32_context.disk, &g_fat32_context.fs_info);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to read boot sector\n");
        return result;
    }
    
    g_fat32_context.mounted = 1;
    
    logger("FAT32: File system mounted successfully\n");
    fat32_boot_print_layout(&g_fat32_context.fs_info);
    
    return FAT32_OK;
}

fat32_error_t fat32_unmount(void)
{
    if (!g_fat32_context.initialized || !g_fat32_context.mounted) {
        return FAT32_OK;
    }
    
    // 刷新缓存
    fat32_error_t result = fat32_cache_flush(g_fat32_context.cache_mgr, 
                                            g_fat32_context.disk, 
                                            &g_fat32_context.fs_info);
    if (result != FAT32_OK) {
        logger("FAT32: Warning - Failed to flush cache during unmount\n");
    }
    
    // 更新FSInfo
    result = fat32_boot_write_fsinfo(g_fat32_context.disk, &g_fat32_context.fs_info);
    if (result != FAT32_OK) {
        logger("FAT32: Warning - Failed to update FSInfo during unmount\n");
    }
    
    // 同步磁盘
    fat32_disk_sync(g_fat32_context.disk);
    
    g_fat32_context.mounted = 0;
    
    logger("FAT32: File system unmounted successfully\n");
    return FAT32_OK;
}

fat32_error_t fat32_cleanup(void)
{
    if (!g_fat32_context.initialized) {
        return FAT32_OK;
    }
    
    // 卸载文件系统
    fat32_unmount();
    
    // 清理缓存
    fat32_cleanup_global_cache(g_fat32_context.disk, &g_fat32_context.fs_info);
    
    // 清理磁盘
    fat32_disk_cleanup(g_fat32_context.disk);
    
    // 清零上下文
    memset(&g_fat32_context, 0, sizeof(fat32_context_t));
    
    logger("FAT32: File system cleaned up\n");
    return FAT32_OK;
}

/* ============================================================================
 * 兼容性接口函数实现
 * ============================================================================ */

int32_t fat32_open(const char *name)
{
    if (!fat32_is_mounted()) {
        return -1;
    }
    
    fat32_file_handle_t *handle;
    fat32_error_t result = fat32_file_open(g_fat32_context.disk, 
                                          &g_fat32_context.fs_info,
                                          name, 
                                          FAT32_O_RDWR | FAT32_O_CREAT,
                                          &handle);
    
    if (result != FAT32_OK) {
        return -1;
    }
    
    // 简化实现：返回句柄指针作为文件描述符
    // 实际实现中应该使用文件描述符表
    return (int32_t)(uintptr_t)handle;
}

int32_t fat32_close(int32_t fd)
{
    if (!fat32_is_mounted() || fd <= 0) {
        return -1;
    }
    
    fat32_file_handle_t *handle = (fat32_file_handle_t *)(uintptr_t)fd;
    fat32_error_t result = fat32_file_close(g_fat32_context.disk, 
                                           &g_fat32_context.fs_info,
                                           handle);
    
    return (result == FAT32_OK) ? 0 : -1;
}

size_t fat32_read(int32_t fd, void *buf, size_t count)
{
    if (!fat32_is_mounted() || fd <= 0 || buf == NULL) {
        return 0;
    }
    
    fat32_file_handle_t *handle = (fat32_file_handle_t *)(uintptr_t)fd;
    uint32_t bytes_read;
    fat32_error_t result = fat32_file_read(g_fat32_context.disk,
                                          &g_fat32_context.fs_info,
                                          handle,
                                          buf,
                                          (uint32_t)count,
                                          &bytes_read);
    
    return (result == FAT32_OK) ? bytes_read : 0;
}

size_t fat32_write(int32_t fd, const void *buf, size_t count)
{
    if (!fat32_is_mounted() || fd <= 0 || buf == NULL) {
        return 0;
    }

    fat32_file_handle_t *handle = (fat32_file_handle_t *)(uintptr_t)fd;
    uint32_t bytes_written;
    fat32_error_t result = fat32_file_write(g_fat32_context.disk,
                                           &g_fat32_context.fs_info,
                                           handle,
                                           buf,
                                           (uint32_t)count,
                                           &bytes_written);

    if (result != FAT32_OK) {
        logger("FAT32: Write failed: %s\n", fat32_get_error_string(result));
        return 0;
    }

    return bytes_written;
}

off_t fat32_lseek(int32_t fd, off_t offset, int32_t whence)
{
    if (!fat32_is_mounted() || fd <= 0) {
        return -1;
    }
    
    fat32_file_handle_t *handle = (fat32_file_handle_t *)(uintptr_t)fd;
    uint32_t new_position;
    fat32_error_t result = fat32_file_seek(handle, (int32_t)offset, whence, &new_position);
    
    return (result == FAT32_OK) ? (off_t)new_position : -1;
}

int32_t fat32_unlink(const char *name)
{
    if (!fat32_is_mounted() || name == NULL) {
        return -1;
    }
    
    fat32_error_t result = fat32_file_delete(g_fat32_context.disk,
                                            &g_fat32_context.fs_info,
                                            name);
    
    return (result == FAT32_OK) ? 0 : -1;
}

/* ============================================================================
 * 内联辅助函数实现
 * ============================================================================ */

uint8_t fat32_is_initialized(void)
{
    return g_fat32_context.initialized;
}

uint8_t fat32_is_mounted(void)
{
    return g_fat32_context.initialized && g_fat32_context.mounted;
}

fat32_context_t *fat32_get_context(void)
{
    return &g_fat32_context;
}

int32_t fat32_error_to_errno(fat32_error_t fat32_error)
{
    switch (fat32_error) {
        case FAT32_OK:
            return 0;
        case FAT32_ERROR_NOT_FOUND:
            return -2;  // ENOENT
        case FAT32_ERROR_ACCESS_DENIED:
            return -13; // EACCES
        case FAT32_ERROR_NO_SPACE:
            return -28; // ENOSPC
        case FAT32_ERROR_ALREADY_EXISTS:
            return -17; // EEXIST
        case FAT32_ERROR_INVALID_PARAM:
        case FAT32_ERROR_INVALID_NAME:
            return -22; // EINVAL
        case FAT32_ERROR_TOO_MANY_OPEN_FILES:
            return -24; // EMFILE
        default:
            return -1;  // 通用错误
    }
}

const char *fat32_get_error_string(fat32_error_t error_code)
{
    switch (error_code) {
        case FAT32_OK:
            return "Success";
        case FAT32_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case FAT32_ERROR_NOT_MOUNTED:
            return "File system not mounted";
        case FAT32_ERROR_DISK_ERROR:
            return "Disk error";
        case FAT32_ERROR_NOT_FOUND:
            return "File or directory not found";
        case FAT32_ERROR_ALREADY_EXISTS:
            return "File or directory already exists";
        case FAT32_ERROR_NO_SPACE:
            return "No space left on device";
        case FAT32_ERROR_ACCESS_DENIED:
            return "Access denied";
        case FAT32_ERROR_INVALID_NAME:
            return "Invalid file name";
        case FAT32_ERROR_TOO_MANY_OPEN_FILES:
            return "Too many open files";
        case FAT32_ERROR_END_OF_FILE:
            return "End of file";
        case FAT32_ERROR_DIRECTORY_NOT_EMPTY:
            return "Directory not empty";
        case FAT32_ERROR_NOT_A_FILE:
            return "Not a file";
        case FAT32_ERROR_NOT_A_DIRECTORY:
            return "Not a directory";
        case FAT32_ERROR_CORRUPTED:
            return "File system corrupted";
        default:
            return "Unknown error";
    }
}

/* ============================================================================
 * 调试和诊断函数实现
 * ============================================================================ */

void fat32_print_fs_info(void)
{
    if (!fat32_is_mounted()) {
        logger("FAT32: File system not mounted\n");
        return;
    }

    logger("=== FAT32 File System Information ===\n");
    fat32_boot_print_info(&g_fat32_context.fs_info.boot_sector);
    fat32_boot_print_layout(&g_fat32_context.fs_info);

    // 打印磁盘统计信息
    uint32_t read_count, write_count, error_count;
    if (fat32_disk_get_stats(g_fat32_context.disk, &read_count, &write_count, &error_count) == FAT32_OK) {
        logger("Disk Statistics:\n");
        logger("  Read operations: %u\n", read_count);
        logger("  Write operations: %u\n", write_count);
        logger("  Error count: %u\n", error_count);
    }

    logger("=====================================\n");
}

void fat32_print_cache_stats(void)
{
    if (!fat32_is_initialized()) {
        logger("FAT32: File system not initialized\n");
        return;
    }

    uint32_t total_blocks, used_blocks, dirty_blocks;
    if (fat32_cache_get_stats(g_fat32_context.cache_mgr, &total_blocks, &used_blocks, &dirty_blocks) == FAT32_OK) {
        logger("=== FAT32 Cache Statistics ===\n");
        logger("Total cache blocks: %u\n", total_blocks);
        logger("Used cache blocks: %u\n", used_blocks);
        logger("Dirty cache blocks: %u\n", dirty_blocks);
        logger("Cache utilization: %u%%\n", (used_blocks * 100) / total_blocks);
        logger("==============================\n");
    } else {
        logger("FAT32: Failed to get cache statistics\n");
    }
}

fat32_error_t fat32_fsck(void)
{
    if (!fat32_is_mounted()) {
        return FAT32_ERROR_NOT_MOUNTED;
    }

    logger("FAT32: Starting file system check...\n");

    // 简化的一致性检查
    fat32_error_t result = FAT32_OK;

    // 检查引导扇区
    result = fat32_boot_validate_boot_sector(&g_fat32_context.fs_info.boot_sector);
    if (result != FAT32_OK) {
        logger("FAT32: Boot sector validation failed\n");
        return result;
    }

    // 检查FSInfo
    result = fat32_boot_validate_fsinfo(&g_fat32_context.fs_info.fsinfo);
    if (result != FAT32_OK) {
        logger("FAT32: FSInfo validation failed\n");
        return result;
    }

    // 简单的FAT表一致性检查
    uint32_t fat_entry;
    result = fat32_fat_read_entry(g_fat32_context.disk, &g_fat32_context.fs_info, 0, &fat_entry);
    if (result != FAT32_OK) {
        logger("FAT32: Failed to read FAT entry 0\n");
        return result;
    }

    if ((fat_entry & 0x0FFFFFF8) != 0x0FFFFFF8) {
        logger("FAT32: Invalid FAT entry 0: 0x%08X\n", fat_entry);
        return FAT32_ERROR_CORRUPTED;
    }

    logger("FAT32: File system check completed successfully\n");
    return FAT32_OK;
}

void fat32_test(void)
{
    logger("=== FAT32 File System Test ===\n");

    // 初始化文件系统
    fat32_error_t result = fat32_init();
    if (result != FAT32_OK) {
        logger("FAT32: Initialization failed: %s\n", fat32_get_error_string(result));
        return;
    }

    // 格式化并挂载
    result = fat32_format_and_mount("TESTVOLUME");
    if (result != FAT32_OK) {
        logger("FAT32: Format and mount failed: %s\n", fat32_get_error_string(result));
        fat32_cleanup();
        return;
    }

    // 打印文件系统信息
    fat32_print_fs_info();

    // 执行一致性检查
    result = fat32_fsck();
    if (result != FAT32_OK) {
        logger("FAT32: File system check failed: %s\n", fat32_get_error_string(result));
    }

    // 测试文件操作
    logger("Testing file operations...\n");

    int32_t fd = fat32_open("/test.txt");
    if (fd > 0) {
        logger("FAT32: File opened successfully, fd=%d\n", fd);

        // 测试读取（文件为空，应该读取0字节）
        char buffer[100];
        size_t bytes_read = fat32_read(fd, buffer, sizeof(buffer));
        logger("FAT32: Read %zu bytes from empty file\n", bytes_read);

        // 关闭文件
        int32_t close_result = fat32_close(fd);
        logger("FAT32: File close result: %d\n", close_result);
    } else {
        logger("FAT32: Failed to open test file\n");
    }

    // 打印缓存统计
    fat32_print_cache_stats();

    // 卸载并清理
    result = fat32_unmount();
    if (result != FAT32_OK) {
        logger("FAT32: Unmount failed: %s\n", fat32_get_error_string(result));
    }

    result = fat32_cleanup();
    if (result != FAT32_OK) {
        logger("FAT32: Cleanup failed: %s\n", fat32_get_error_string(result));
    }

    logger("=== FAT32 Test Completed ===\n");
}

/* ============================================================================
 * 扩展功能函数实现
 * ============================================================================ */

fat32_error_t fat32_mkdir(const char *dirname)
{
    if (!fat32_is_mounted() || dirname == NULL) {
        return FAT32_ERROR_NOT_MOUNTED;
    }

    // 简化实现：只支持在根目录创建
    uint32_t new_dir_cluster;
    return fat32_dir_create_directory(g_fat32_context.disk,
                                     &g_fat32_context.fs_info,
                                     g_fat32_context.fs_info.boot_sector.root_cluster,
                                     dirname,
                                     &new_dir_cluster);
}

fat32_error_t fat32_rmdir(const char *dirname)
{
    if (!fat32_is_mounted() || dirname == NULL) {
        return FAT32_ERROR_NOT_MOUNTED;
    }

    // 简化实现：只支持在根目录删除
    return fat32_dir_remove_directory(g_fat32_context.disk,
                                     &g_fat32_context.fs_info,
                                     g_fat32_context.fs_info.boot_sector.root_cluster,
                                     dirname);
}

fat32_error_t fat32_listdir(const char *dirname,
                            fat32_dir_entry_t *entries,
                            uint32_t max_entries,
                            uint32_t *entry_count)
{
    if (!fat32_is_mounted() || dirname == NULL || entries == NULL || entry_count == NULL) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    *entry_count = 0;

    // 简化实现：只支持列出根目录
    if (strcmp(dirname, "/") != 0) {
        return FAT32_ERROR_NOT_FOUND;
    }

    fat32_dir_iterator_t iterator;
    fat32_dir_iterator_init(&iterator, g_fat32_context.fs_info.boot_sector.root_cluster);

    while (!iterator.end_of_dir && *entry_count < max_entries) {
        fat32_dir_entry_t dir_entry;
        fat32_error_t result = fat32_dir_iterator_next(g_fat32_context.disk,
                                                      &g_fat32_context.fs_info,
                                                      &iterator,
                                                      &dir_entry);
        if (result != FAT32_OK) {
            if (result == FAT32_ERROR_END_OF_FILE) {
                break;
            }
            return result;
        }

        // 跳过空闲和已删除的目录项
        if (fat32_dir_is_free_entry(&dir_entry) || fat32_dir_is_deleted_entry(&dir_entry)) {
            continue;
        }

        entries[*entry_count] = dir_entry;
        (*entry_count)++;
    }

    return FAT32_OK;
}

fat32_error_t fat32_stat(const char *filepath, fat32_dir_entry_t *file_info)
{
    if (!fat32_is_mounted() || filepath == NULL || file_info == NULL) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    return fat32_file_stat(g_fat32_context.disk,
                          &g_fat32_context.fs_info,
                          filepath,
                          file_info);
}

fat32_error_t fat32_rename(const char *old_name, const char *new_name)
{
    if (!fat32_is_mounted() || old_name == NULL || new_name == NULL) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    return fat32_file_rename(g_fat32_context.disk,
                            &g_fat32_context.fs_info,
                            old_name,
                            new_name);
}
