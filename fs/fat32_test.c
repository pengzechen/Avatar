/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file fat32_test.c
 * @brief Implementation of fat32_test.c
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file fat32_test.c
 * @brief FAT32文件系统测试程序
 * 
 * 本文件包含了FAT32文件系统的测试函数，用于验证各项功能的正确性。
 */

#include "fs/fat32.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "os_cfg.h"
#include "mem/mem.h"

/**
 * @brief 测试基本的文件系统操作
 */
void
fat32_test_basic_operations(void)
{
    logger("=== Testing Basic FAT32 Operations ===\n");

    // 测试初始化
    logger("1. Testing initialization...\n");
    fat32_error_t result = fat32_init();
    if (result != FAT32_OK) {
        logger("   FAILED: Initialization failed: %s\n", fat32_get_error_string(result));
        return;
    }
    logger("   PASSED: Initialization successful\n");

    // 测试格式化和挂载
    logger("2. Testing format and mount...\n");
    result = fat32_format_and_mount("TESTFS");
    if (result != FAT32_OK) {
        logger("   FAILED: Format and mount failed: %s\n", fat32_get_error_string(result));
        fat32_cleanup();
        return;
    }
    logger("   PASSED: Format and mount successful\n");

    // 测试文件系统信息
    logger("3. Testing filesystem info...\n");
    fat32_print_fs_info();

    // 测试一致性检查
    logger("4. Testing filesystem check...\n");
    result = fat32_fsck();
    if (result != FAT32_OK) {
        logger("   FAILED: Filesystem check failed: %s\n", fat32_get_error_string(result));
    } else {
        logger("   PASSED: Filesystem check successful\n");
    }

    // 清理
    logger("5. Testing cleanup...\n");
    result = fat32_cleanup();
    if (result != FAT32_OK) {
        logger("   FAILED: Cleanup failed: %s\n", fat32_get_error_string(result));
    } else {
        logger("   PASSED: Cleanup successful\n");
    }

    logger("=== Basic Operations Test Completed ===\n\n");
}

/**
 * @brief 测试文件操作
 */
void
fat32_test_file_operations(void)
{
    logger("=== Testing FAT32 File Operations ===\n");

    // 初始化和挂载
    fat32_error_t result = fat32_init();
    if (result != FAT32_OK) {
        logger("FAILED: Cannot initialize filesystem\n");
        return;
    }

    result = fat32_format_and_mount("FILETEST");
    if (result != FAT32_OK) {
        logger("FAILED: Cannot format and mount filesystem\n");
        fat32_cleanup();
        return;
    }

    // 测试文件创建
    logger("1. Testing file creation...\n");
    int32_t fd = fat32_open("/test.txt");
    if (fd > 0) {
        logger("   PASSED: File created successfully, fd=%d\n", fd);

        // 测试文件读取（空文件）
        logger("2. Testing file read (empty file)...\n");
        char   buffer[100];
        size_t bytes_read = fat32_read(fd, buffer, sizeof(buffer));
        if (bytes_read == 0) {
            logger("   PASSED: Read 0 bytes from empty file\n");
        } else {
            logger("   WARNING: Read %zu bytes from empty file (expected 0)\n", bytes_read);
        }

        // 测试文件定位
        logger("3. Testing file seek...\n");
        off_t pos = fat32_lseek(fd, 0, FAT32_SEEK_SET);
        if (pos == 0) {
            logger("   PASSED: Seek to beginning successful\n");
        } else {
            logger("   FAILED: Seek returned %ld (expected 0)\n", pos);
        }

        // 测试文件关闭
        logger("4. Testing file close...\n");
        int32_t close_result = fat32_close(fd);
        if (close_result == 0) {
            logger("   PASSED: File closed successfully\n");
        } else {
            logger("   FAILED: File close failed\n");
        }

    } else {
        logger("   FAILED: Cannot create file\n");
    }

    // 测试文件状态
    logger("5. Testing file stat...\n");
    fat32_dir_entry_t file_info;
    result = fat32_stat("/test.txt", &file_info);
    if (result == FAT32_OK) {
        logger("   PASSED: File stat successful\n");
        logger("   File size: %u bytes\n", file_info.file_size);
        logger("   File attributes: 0x%02X\n", file_info.attr);
    } else {
        logger("   FAILED: File stat failed: %s\n", fat32_get_error_string(result));
    }

    // 测试文件删除
    logger("6. Testing file deletion...\n");
    int32_t unlink_result = fat32_unlink("/test.txt");
    if (unlink_result == 0) {
        logger("   PASSED: File deleted successfully\n");
    } else {
        logger("   FAILED: File deletion failed\n");
    }

    // 验证文件已删除
    result = fat32_stat("/test.txt", &file_info);
    if (result == FAT32_ERROR_NOT_FOUND) {
        logger("   PASSED: File no longer exists after deletion\n");
    } else {
        logger("   WARNING: File still exists after deletion\n");
    }

    // 清理
    fat32_cleanup();
    logger("=== File Operations Test Completed ===\n\n");
}

/**
 * @brief 测试目录操作
 */
void
fat32_test_directory_operations(void)
{
    logger("=== Testing FAT32 Directory Operations ===\n");

    // 初始化和挂载
    fat32_error_t result = fat32_init();
    if (result != FAT32_OK) {
        logger("FAILED: Cannot initialize filesystem\n");
        return;
    }

    result = fat32_format_and_mount("DIRTEST");
    if (result != FAT32_OK) {
        logger("FAILED: Cannot format and mount filesystem\n");
        fat32_cleanup();
        return;
    }

    // 测试目录列表（根目录）
    logger("1. Testing directory listing (root)...\n");
    fat32_dir_entry_t entries[10];
    uint32_t          entry_count;
    result = fat32_listdir("/", entries, 10, &entry_count);
    if (result == FAT32_OK) {
        logger("   PASSED: Root directory listed, %u entries found\n", entry_count);
        for (uint32_t i = 0; i < entry_count; i++) {
            char filename[13];
            fat32_dir_convert_from_dir_entry(&entries[i], filename, sizeof(filename));
            logger("   Entry %u: %s (%u bytes)\n", i, filename, entries[i].file_size);
        }
    } else {
        logger("   FAILED: Directory listing failed: %s\n", fat32_get_error_string(result));
    }

    // 测试目录创建
    logger("2. Testing directory creation...\n");
    result = fat32_mkdir("testdir");
    if (result == FAT32_OK) {
        logger("   PASSED: Directory created successfully\n");
    } else {
        logger("   FAILED: Directory creation failed: %s\n", fat32_get_error_string(result));
    }

    // 再次列出根目录，验证新目录
    logger("3. Testing directory listing after creation...\n");
    result = fat32_listdir("/", entries, 10, &entry_count);
    if (result == FAT32_OK) {
        logger("   PASSED: Root directory listed, %u entries found\n", entry_count);
        uint8_t found_testdir = 0;
        for (uint32_t i = 0; i < entry_count; i++) {
            char filename[13];
            fat32_dir_convert_from_dir_entry(&entries[i], filename, sizeof(filename));
            logger("   Entry %u: %s (%s)\n",
                   i,
                   filename,
                   (entries[i].attr & FAT32_ATTR_DIRECTORY) ? "DIR" : "FILE");
            if (strcmp(filename, "TESTDIR") == 0) {
                found_testdir = 1;
            }
        }
        if (found_testdir) {
            logger("   PASSED: New directory found in listing\n");
        } else {
            logger("   WARNING: New directory not found in listing\n");
        }
    } else {
        logger("   FAILED: Directory listing failed: %s\n", fat32_get_error_string(result));
    }

    // 测试目录删除
    logger("4. Testing directory deletion...\n");
    result = fat32_rmdir("testdir");
    if (result == FAT32_OK) {
        logger("   PASSED: Directory deleted successfully\n");
    } else {
        logger("   FAILED: Directory deletion failed: %s\n", fat32_get_error_string(result));
    }

    // 清理
    fat32_cleanup();
    logger("=== Directory Operations Test Completed ===\n\n");
}

/**
 * @brief 测试文件写入功能
 */
void
fat32_test_file_write_operations(void)
{
    logger("=== Testing FAT32 File Write Operations ===\n");

    // 初始化和挂载
    fat32_error_t result = fat32_init();
    if (result != FAT32_OK) {
        logger("FAILED: Cannot initialize filesystem\n");
        return;
    }

    result = fat32_format_and_mount("WRITETEST");
    if (result != FAT32_OK) {
        logger("FAILED: Cannot format and mount filesystem\n");
        fat32_cleanup();
        return;
    }

    // 测试1：创建文件并写入小量数据
    logger("1. Testing small file write...\n");
    int32_t fd = fat32_open("/small.txt");
    if (fd > 0) {
        const char *test_data = "Hello, FAT32 World!";
        size_t      data_len  = strlen(test_data);

        size_t written = fat32_write(fd, test_data, data_len);
        if (written == data_len) {
            logger("   PASSED: Wrote %zu bytes successfully\n", written);

            // 验证写入的数据
            fat32_lseek(fd, 0, FAT32_SEEK_SET);
            char   read_buffer[100];
            size_t read_bytes       = fat32_read(fd, read_buffer, sizeof(read_buffer) - 1);
            read_buffer[read_bytes] = '\0';

            if (read_bytes == data_len && strcmp(read_buffer, test_data) == 0) {
                logger("   PASSED: Read back data matches written data\n");
            } else {
                logger("   FAILED: Read back data mismatch. Expected '%s', got '%s'\n",
                       test_data,
                       read_buffer);
            }
        } else {
            logger("   FAILED: Write returned %zu bytes (expected %zu)\n", written, data_len);
        }

        fat32_close(fd);
    } else {
        logger("   FAILED: Cannot create test file\n");
    }

    // 测试2：写入大量数据（跨多个簇）
    logger("2. Testing large file write (multi-cluster)...\n");
    fd = fat32_open("/large.txt");
    if (fd > 0) {
        // 创建8KB的测试数据（2个簇）
        const uint32_t large_size = 8192;
        char *large_buffer        = (char *) kalloc_pages((large_size + PAGE_SIZE - 1) / PAGE_SIZE);
        if (large_buffer != NULL) {
            // 填充测试模式
            for (uint32_t i = 0; i < large_size; i++) {
                large_buffer[i] = 'A' + (i % 26);
            }

            size_t written = fat32_write(fd, large_buffer, large_size);
            if (written == large_size) {
                logger("   PASSED: Wrote %u bytes across multiple clusters\n", large_size);

                // 验证文件大小
                fat32_dir_entry_t file_info;
                fat32_close(fd);  // 关闭以更新目录项

                result = fat32_stat("/large.txt", &file_info);
                if (result == FAT32_OK && file_info.file_size == large_size) {
                    logger("   PASSED: File size correctly updated to %u bytes\n",
                           file_info.file_size);
                } else {
                    logger("   FAILED: File size mismatch. Expected %u, got %u\n",
                           large_size,
                           file_info.file_size);
                }

                // 重新打开并验证数据
                fd = fat32_open("/large.txt");
                if (fd > 0) {
                    char *verify_buffer =
                        (char *) kalloc_pages((large_size + PAGE_SIZE - 1) / PAGE_SIZE);
                    if (verify_buffer != NULL) {
                        size_t read_bytes = fat32_read(fd, verify_buffer, large_size);
                        if (read_bytes == large_size &&
                            memcmp(large_buffer, verify_buffer, large_size) == 0) {
                            logger("   PASSED: Large file data verification successful\n");
                        } else {
                            logger("   FAILED: Large file data verification failed\n");
                        }
                        kfree_pages(verify_buffer, (large_size + PAGE_SIZE - 1) / PAGE_SIZE);
                    }
                    fat32_close(fd);
                }
            } else {
                logger("   FAILED: Large write returned %zu bytes (expected %u)\n",
                       written,
                       large_size);
                fat32_close(fd);
            }

            kfree_pages(large_buffer, (large_size + PAGE_SIZE - 1) / PAGE_SIZE);
        } else {
            logger("   FAILED: Cannot allocate large buffer\n");
            fat32_close(fd);
        }
    } else {
        logger("   FAILED: Cannot create large test file\n");
    }

    // 测试3：追加写入
    logger("3. Testing append write...\n");
    fd = fat32_open("/append.txt");
    if (fd > 0) {
        // 先写入一些数据
        const char *first_data = "First line\n";
        size_t      written1   = fat32_write(fd, first_data, strlen(first_data));

        // 追加更多数据
        const char *second_data = "Second line\n";
        size_t      written2    = fat32_write(fd, second_data, strlen(second_data));

        if (written1 == strlen(first_data) && written2 == strlen(second_data)) {
            logger("   PASSED: Append write successful\n");

            // 验证完整内容
            fat32_lseek(fd, 0, FAT32_SEEK_SET);
            char   read_buffer[100];
            size_t total_read       = fat32_read(fd, read_buffer, sizeof(read_buffer) - 1);
            read_buffer[total_read] = '\0';

            char expected[100];
            strcpy(expected, first_data);
            strcat(expected, second_data);

            if (strcmp(read_buffer, expected) == 0) {
                logger("   PASSED: Append data verification successful\n");
            } else {
                logger("   FAILED: Append data mismatch\n");
                logger("   Expected: '%s'\n", expected);
                logger("   Got: '%s'\n", read_buffer);
            }
        } else {
            logger("   FAILED: Append write failed\n");
        }

        fat32_close(fd);
    } else {
        logger("   FAILED: Cannot create append test file\n");
    }

    // 测试4：文件截断
    logger("4. Testing file truncation...\n");
    fd = fat32_open("/truncate.txt");
    if (fd > 0) {
        // 写入一些数据
        const char *original_data = "This is a long line that will be truncated";
        size_t      written       = fat32_write(fd, original_data, strlen(original_data));

        if (written == strlen(original_data)) {
            // 截断文件
            fat32_error_t trunc_result = fat32_file_truncate(g_fat32_context.disk,
                                                             &g_fat32_context.fs_info,
                                                             (fat32_file_handle_t *) (uint64_t) fd,
                                                             10);  // 截断到10字节

            if (trunc_result == FAT32_OK) {
                logger("   PASSED: File truncation successful\n");

                // 验证截断后的大小
                fat32_lseek(fd, 0, FAT32_SEEK_SET);
                char   trunc_buffer[20];
                size_t read_bytes = fat32_read(fd, trunc_buffer, sizeof(trunc_buffer));

                if (read_bytes == 10) {
                    logger("   PASSED: Truncated file size correct (%zu bytes)\n", read_bytes);
                } else {
                    logger("   FAILED: Truncated file size incorrect (%zu bytes, expected 10)\n",
                           read_bytes);
                }
            } else {
                logger("   FAILED: File truncation failed: %s\n",
                       fat32_get_error_string(trunc_result));
            }
        }

        fat32_close(fd);
    } else {
        logger("   FAILED: Cannot create truncation test file\n");
    }

    // 清理测试文件
    logger("5. Cleaning up test files...\n");
    fat32_unlink("/small.txt");
    fat32_unlink("/large.txt");
    fat32_unlink("/append.txt");
    fat32_unlink("/truncate.txt");

    // 清理
    fat32_cleanup();
    logger("=== File Write Operations Test Completed ===\n\n");
}

/**
 * @brief 运行所有FAT32测试
 */
void
fat32_run_all_tests(void)
{
    logger("========================================\n");
    logger("         FAT32 File System Tests       \n");
    logger("========================================\n\n");

    fat32_test_basic_operations();
    fat32_test_file_operations();
    fat32_test_file_write_operations();  // 新增的写入测试
    fat32_test_directory_operations();

    logger("========================================\n");
    logger("         All Tests Completed           \n");
    logger("========================================\n");
}
