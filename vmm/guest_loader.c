/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file guest_loader.c
 * @brief Implementation of guest_loader.c
 * @author Avatar Project Team
 * @date 2024
 */

#include "vmm/guest_loader.h"
#include "fs/fat32.h"
#include "mem/mem.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "fs/fat32.h"
#include "fs/fat32_file.h"
#include "timer.h"

// 从文件系统加载文件到内存
fat32_error_t
guest_load_file_to_memory(const char *filepath, uint64_t load_addr, size_t *loaded_size)
{
    if (!filepath || !loaded_size) {
        logger_error("Invalid parameters for file loading\n");
        return FAT32_ERROR_INVALID_PARAM;
    }

    *loaded_size = 0;

    if (!fat32_is_mounted()) {
        logger_error("File system not mounted\n");
        return FAT32_ERROR_NOT_MOUNTED;
    }

    logger_info("Loading file: %s to 0x%llx\n", filepath, load_addr);

    int32_t fd = fat32_open_readonly(filepath);
    if (fd <= 0) {
        logger_warn("Failed to open file: %s\n", filepath);
        return FAT32_ERROR_NOT_FOUND;
    }

    // 获取文件大小
    fat32_lseek(fd, 0, FAT32_SEEK_END);
    off_t file_size_off = fat32_lseek(fd, 0, FAT32_SEEK_CUR);  // 获取当前位置（即文件大小）
    fat32_lseek(fd, 0, FAT32_SEEK_SET);

    if (file_size_off < 0) {
        fat32_close(fd);
        logger_error("Failed to get file size: %s\n", filepath);
        return FAT32_ERROR_DISK_ERROR;
    }

    size_t file_size = (size_t) file_size_off;

    if (file_size == 0) {
        fat32_close(fd);
        logger_warn("File is empty: %s\n", filepath);
        return FAT32_ERROR_INVALID_PARAM;
    }

    logger_info("File size: %zu bytes\n", file_size);

    // 分配临时缓冲区
    size_t pages_needed = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;
    void  *temp_buffer  = kalloc_pages(pages_needed);
    if (!temp_buffer) {
        fat32_close(fd);
        logger_error("Failed to allocate %zu pages for file: %s\n", pages_needed, filepath);
        return FAT32_ERROR_NO_SPACE;
    }

    // 记录开始时间用于性能测量（只测量实际文件读取）
    uint64_t start_ticks = read_cntpct_el0();
    uint64_t frequency   = read_cntfrq_el0();

    // 读取文件
    size_t bytes_read = fat32_read(fd, temp_buffer, file_size);

    // 记录结束时间
    uint64_t end_ticks = read_cntpct_el0();

    fat32_close(fd);

    if (bytes_read != file_size) {
        kfree_pages(temp_buffer, pages_needed);
        logger_error("Failed to read complete file: %s (read %zu of %zu bytes)\n",
                     filepath,
                     bytes_read,
                     file_size);
        return FAT32_ERROR_DISK_ERROR;
    }

    // 复制到目标地址
    memcpy((void *) load_addr, temp_buffer, file_size);
    kfree_pages(temp_buffer, pages_needed);

    *loaded_size = file_size;

    // 计算并报告性能（只包含实际文件读取时间）
    uint64_t duration_ticks = end_ticks - start_ticks;
    uint64_t duration_us    = (duration_ticks * 1000000) / frequency;  // 转换为微秒
    uint64_t throughput_kbps =
        (file_size * 1000) / (duration_us + 1);  // +1 to avoid division by zero

    logger_info("Successfully loaded %s: %zu bytes to 0x%llx\n", filepath, file_size, load_addr);
    logger_info("File read time: %llu us (%llu ticks), Throughput: %llu KB/s\n",
                duration_us,
                duration_ticks,
                throughput_kbps);
    return FAT32_OK;
}

// 验证Guest文件是否存在
bool
guest_validate_files(const guest_manifest_t *manifest)
{
    if (!manifest) {
        return false;
    }

    if (!fat32_is_mounted()) {
        logger_error("File system not mounted for validation\n");
        return false;
    }

    // 检查内核文件（必需）
    if (!manifest->files.kernel_path) {
        logger_error("Kernel path not specified for guest: %s\n", manifest->name);
        return false;
    }

    int32_t fd = fat32_open_readonly(manifest->files.kernel_path);
    if (fd <= 0) {
        logger_error("Kernel file not found: %s\n", manifest->files.kernel_path);
        return false;
    }
    fat32_close(fd);

    // 检查DTB文件（如果需要）
    if (manifest->files.needs_dtb && manifest->files.dtb_path) {
        fd = fat32_open_readonly(manifest->files.dtb_path);
        if (fd <= 0) {
            logger_warn("DTB file not found: %s\n", manifest->files.dtb_path);
            return false;
        }
        fat32_close(fd);
    }

    // 检查initrd文件（如果需要）
    if (manifest->files.needs_initrd && manifest->files.initrd_path) {
        fd = fat32_open_readonly(manifest->files.initrd_path);
        if (fd <= 0) {
            logger_warn("Initrd file not found: %s\n", manifest->files.initrd_path);
            return false;
        }
        fat32_close(fd);
    }

    logger_info("All required files validated for guest: %s\n", manifest->name);
    return true;
}

// 获取Guest文件的大小信息
bool
guest_get_file_sizes(const guest_manifest_t *manifest,
                     size_t                 *kernel_size,
                     size_t                 *dtb_size,
                     size_t                 *initrd_size)
{
    if (!manifest || !kernel_size || !dtb_size || !initrd_size) {
        return false;
    }

    *kernel_size = 0;
    *dtb_size    = 0;
    *initrd_size = 0;

    if (!fat32_is_mounted()) {
        return false;
    }

    // 获取内核文件大小
    if (manifest->files.kernel_path) {
        int32_t fd = fat32_open_readonly(manifest->files.kernel_path);
        if (fd > 0) {
            fat32_lseek(fd, 0, FAT32_SEEK_END);
            off_t size_off = fat32_lseek(fd, 0, FAT32_SEEK_CUR);
            if (size_off >= 0) {
                *kernel_size = (size_t) size_off;
            }
            fat32_close(fd);
        }
    }

    // 获取DTB文件大小
    if (manifest->files.needs_dtb && manifest->files.dtb_path) {
        int32_t fd = fat32_open_readonly(manifest->files.dtb_path);
        if (fd > 0) {
            fat32_lseek(fd, 0, FAT32_SEEK_END);
            off_t size_off = fat32_lseek(fd, 0, FAT32_SEEK_CUR);
            if (size_off >= 0) {
                *dtb_size = (size_t) size_off;
            }
            fat32_close(fd);
        }
    }

    // 获取initrd文件大小
    if (manifest->files.needs_initrd && manifest->files.initrd_path) {
        int32_t fd = fat32_open_readonly(manifest->files.initrd_path);
        if (fd > 0) {
            fat32_lseek(fd, 0, FAT32_SEEK_END);
            off_t size_off = fat32_lseek(fd, 0, FAT32_SEEK_CUR);
            if (size_off >= 0) {
                *initrd_size = (size_t) size_off;
            }
            fat32_close(fd);
        }
    }

    return true;
}

// 从文件系统加载Guest镜像
guest_load_result_t
guest_load_from_manifest(const guest_manifest_t *manifest)
{
    guest_load_result_t result = {0};

    if (!manifest) {
        result.error = GUEST_LOAD_ERROR_INVALID_PARAM;
        return result;
    }

    logger_info("Loading guest: %s (type: %s)\n",
                manifest->name,
                guest_type_to_string(manifest->type));

    // 验证文件存在性
    if (!guest_validate_files(manifest)) {
        logger_error("File validation failed for guest: %s\n", manifest->name);
        result.error = GUEST_LOAD_ERROR_FILE_SYSTEM_ERROR;
        return result;
    }

    // 1. 加载内核（必需）
    fat32_error_t fs_result = guest_load_file_to_memory(manifest->files.kernel_path,
                                                        manifest->bin_loadaddr,
                                                        &result.kernel_size);

    if (fs_result != FAT32_OK) {
        logger_error("Failed to load kernel: %s (error: %d)\n",
                     manifest->files.kernel_path,
                     fs_result);
        result.error = GUEST_LOAD_ERROR_KERNEL_LOAD_FAILED;
        return result;
    }

    // 2. 加载DTB（可选）
    if (manifest->files.needs_dtb && manifest->files.dtb_path) {
        fs_result = guest_load_file_to_memory(manifest->files.dtb_path,
                                              manifest->dtb_loadaddr,
                                              &result.dtb_size);

        if (fs_result != FAT32_OK) {
            logger_warn("Failed to load DTB: %s (error: %d)\n",
                        manifest->files.dtb_path,
                        fs_result);
            // DTB加载失败不是致命错误，继续执行
        }
    }

    // 3. 加载initrd（可选）
    if (manifest->files.needs_initrd && manifest->files.initrd_path) {
        fs_result = guest_load_file_to_memory(manifest->files.initrd_path,
                                              manifest->fs_loadaddr,
                                              &result.initrd_size);

        if (fs_result != FAT32_OK) {
            logger_warn("Failed to load initrd: %s (error: %d)\n",
                        manifest->files.initrd_path,
                        fs_result);
            // initrd加载失败不是致命错误，继续执行
        }
    }

    result.error = GUEST_LOAD_SUCCESS;
    logger_info("Guest %s loaded successfully: kernel=%zu bytes, dtb=%zu bytes, initrd=%zu bytes\n",
                manifest->name,
                result.kernel_size,
                result.dtb_size,
                result.initrd_size);
    return result;
}
