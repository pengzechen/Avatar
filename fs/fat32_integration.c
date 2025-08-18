/**
 * @file fat32_integration.c
 * @brief FAT32文件系统集成示例
 * 
 * 本文件展示如何将FAT32文件系统集成到Avatar操作系统中，
 * 以及如何替换或扩展现有的ramfs。
 */

#include "fs/fat32.h"
#include "fs/fat32_utils.h"
#include "mem/mem.h"
#include "io.h"
#include "os_cfg.h"
#include "lib/avatar_string.h"
#include "mem/mem.h"

// 测试函数声明
void fat32_test_basic_operations(void);
void fat32_test_file_operations(void);
void fat32_test_directory_operations(void);
void fat32_test_file_write_operations(void);
void fat32_run_all_tests(void);
void fat32_write_demo(void);

/**
 * @brief 初始化FAT32文件系统并运行基本测试
 * 
 * 这个函数可以在系统启动时调用，用于初始化FAT32文件系统
 */
void fat32_system_init(void)
{
    logger("Avatar OS: Initializing FAT32 File System...\n");
    
    // 初始化FAT32文件系统
    fat32_error_t result = fat32_init();
    if (result != FAT32_OK) {
        logger("FAT32: Initialization failed: %s\n", fat32_get_error_string(result));
        return;
    }
    
    // 格式化并挂载文件系统
    result = fat32_format_and_mount("AVATARFS");
    if (result != FAT32_OK) {
        logger("FAT32: Format and mount failed: %s\n", fat32_get_error_string(result));
        fat32_cleanup();
        return;
    }
    
    logger("FAT32: File system ready!\n");
    
    // 打印文件系统信息
    fat32_print_fs_info();
    
    // 运行基本功能验证
    logger("FAT32: Running basic functionality test...\n");
    
    // 创建一个测试文件
    int32_t fd = fat32_open("/welcome.txt");
    if (fd > 0) {
        logger("FAT32: Test file created successfully\n");
        fat32_close(fd);
        
        // 列出根目录内容
        fat32_dir_entry_t entries[10];
        uint32_t count;
        result = fat32_listdir("/", entries, 10, &count);
        if (result == FAT32_OK) {
            logger("FAT32: Root directory contains %u entries:\n", count);
            for (uint32_t i = 0; i < count; i++) {
                char filename[13];
                fat32_dir_convert_from_short_name(entries[i].name, filename, sizeof(filename));
                logger("  - %s (%u bytes, %s)\n", 
                       filename, 
                       entries[i].file_size,
                       (entries[i].attr & FAT32_ATTR_DIRECTORY) ? "DIR" : "FILE");
            }
        }
    } else {
        logger("FAT32: Warning - Could not create test file\n");
    }
    
    logger("FAT32: System initialization completed\n");
}

/**
 * @brief 运行完整的FAT32测试套件
 * 
 * 这个函数可以用于系统测试或调试
 */
void fat32_run_system_tests(void)
{
    logger("Avatar OS: Starting FAT32 comprehensive tests...\n");
    
    // 先清理可能存在的实例
    fat32_cleanup();
    
    // 运行所有测试
    fat32_run_all_tests();
    
    logger("Avatar OS: FAT32 tests completed\n");
}

/**
 * @brief 演示FAT32与ramfs的兼容性
 * 
 * 展示如何使用相同的接口操作FAT32文件系统
 */
void fat32_compatibility_demo(void)
{
    logger("=== FAT32 Compatibility Demo ===\n");
    
    // 确保文件系统已初始化
    if (!fat32_is_mounted()) {
        fat32_system_init();
    }
    
    if (!fat32_is_mounted()) {
        logger("FAT32: File system not available for demo\n");
        return;
    }
    
    // 使用标准接口操作文件
    logger("1. Creating and writing to a file...\n");
    
    // 创建文件
    int32_t fd = fat32_open("/demo.txt");
    if (fd > 0) {
        logger("   File opened successfully (fd=%d)\n", fd);
        
        // 写入测试数据
        const char *test_data = "Hello, FAT32 File System!\nThis is a test file.\n";
        size_t written = fat32_write(fd, test_data, strlen(test_data));
        logger("   Attempted to write %zu bytes, actually wrote %zu bytes\n",
               strlen(test_data), written);
        
        // 定位到文件开头
        off_t pos = fat32_lseek(fd, 0, FAT32_SEEK_SET);
        logger("   Seek to beginning: position = %ld\n", pos);
        
        // 尝试读取
        char buffer[100];
        size_t read_bytes = fat32_read(fd, buffer, sizeof(buffer) - 1);
        buffer[read_bytes] = '\0';
        logger("   Read %zu bytes: '%s'\n", read_bytes, buffer);
        
        // 关闭文件
        fat32_close(fd);
        logger("   File closed\n");
        
        // 获取文件信息
        fat32_dir_entry_t file_info;
        fat32_error_t result = fat32_stat("/demo.txt", &file_info);
        if (result == FAT32_OK) {
            logger("   File size: %u bytes\n", file_info.file_size);
            logger("   File attributes: 0x%02X\n", file_info.attr);
        }
        
    } else {
        logger("   Failed to open file\n");
    }
    
    logger("2. Directory operations...\n");
    
    // 创建目录
    fat32_error_t result = fat32_mkdir("demodir");
    if (result == FAT32_OK) {
        logger("   Directory created successfully\n");
        
        // 列出目录内容
        fat32_dir_entry_t entries[10];
        uint32_t count;
        result = fat32_listdir("/", entries, 10, &count);
        if (result == FAT32_OK) {
            logger("   Root directory now contains %u entries\n", count);
        }
        
        // 删除目录
        result = fat32_rmdir("demodir");
        if (result == FAT32_OK) {
            logger("   Directory deleted successfully\n");
        }
    }
    
    logger("3. Error handling demo...\n");
    
    // 尝试访问不存在的文件
    result = fat32_stat("/nonexistent.txt", NULL);
    logger("   Accessing non-existent file: %s\n", fat32_get_error_string(result));
    
    // 尝试删除不存在的文件
    int32_t unlink_result = fat32_unlink("/nonexistent.txt");
    logger("   Deleting non-existent file: %s\n", 
           unlink_result == 0 ? "Success" : "Failed (expected)");
    
    logger("=== Compatibility Demo Completed ===\n");
}

/**
 * @brief 系统关闭时的清理函数
 */
void fat32_system_shutdown(void)
{
    logger("Avatar OS: Shutting down FAT32 file system...\n");
    
    if (fat32_is_mounted()) {
        // 刷新缓存
        fat32_print_cache_stats();
        
        // 卸载文件系统
        fat32_error_t result = fat32_unmount();
        if (result != FAT32_OK) {
            logger("FAT32: Warning - Unmount failed: %s\n", fat32_get_error_string(result));
        }
    }
    
    // 清理资源
    fat32_cleanup();
    
    logger("FAT32: Shutdown completed\n");
}

/**
 * @brief 主要的FAT32演示函数
 * 
 * 这个函数展示了FAT32文件系统的完整使用流程
 */
void fat32_main_demo(void)
{
    logger("\n");
    logger("================================================\n");
    logger("    Avatar OS - FAT32 File System Demo\n");
    logger("================================================\n");
    
    // 1. 系统初始化
    fat32_system_init();
    
    // 2. 兼容性演示
    fat32_compatibility_demo();

    // 3. 写入功能演示
    fat32_write_demo();

    // 4. 运行测试（可选）
    logger("\nRunning comprehensive tests...\n");
    fat32_run_system_tests();
    
    // 4. 系统关闭
    fat32_system_shutdown();
    
    logger("================================================\n");
    logger("           Demo Completed Successfully\n");
    logger("================================================\n\n");
}

/**
 * @brief 简单的性能测试
 */
void fat32_performance_test(void)
{
    logger("=== FAT32 Performance Test ===\n");
    
    if (!fat32_is_mounted()) {
        fat32_system_init();
    }
    
    if (!fat32_is_mounted()) {
        logger("FAT32: File system not available\n");
        return;
    }
    
    // 测试文件创建性能
    logger("Testing file creation performance...\n");
    
    const int num_files = 10;
    char filename[32];
    
    for (int i = 0; i < num_files; i++) {
        my_snprintf(filename, sizeof(filename), "/test%d.txt", i);
        
        int32_t fd = fat32_open(filename);
        if (fd > 0) {
            fat32_close(fd);
            logger("  Created file %s\n", filename);
        } else {
            logger("  Failed to create file %s\n", filename);
        }
    }
    
    // 列出所有文件
    logger("Listing all files...\n");
    fat32_dir_entry_t entries[20];
    uint32_t count;
    fat32_error_t result = fat32_listdir("/", entries, 20, &count);
    if (result == FAT32_OK) {
        logger("Found %u entries in root directory\n", count);
    }
    
    // 清理测试文件
    logger("Cleaning up test files...\n");
    for (int i = 0; i < num_files; i++) {
        my_snprintf(filename, sizeof(filename), "/test%d.txt", i);
        fat32_unlink(filename);
    }
    
    // 打印缓存统计
    fat32_print_cache_stats();
    
    logger("=== Performance Test Completed ===\n");
}

/**
 * @brief 全面的文件写入演示
 */
void fat32_write_demo(void)
{
    logger("=== FAT32 Write Operations Demo ===\n");

    if (!fat32_is_mounted()) {
        fat32_system_init();
    }

    if (!fat32_is_mounted()) {
        logger("FAT32: File system not available\n");
        return;
    }

    // 演示1：创建并写入文本文件
    logger("1. Creating and writing text file...\n");
    int32_t fd = fat32_open("/readme.txt");
    if (fd > 0) {
        const char *content =
            "Welcome to Avatar OS FAT32 File System!\n"
            "========================================\n"
            "\n"
            "This file demonstrates the write capabilities\n"
            "of our FAT32 implementation.\n"
            "\n"
            "Features:\n"
            "- File creation and deletion\n"
            "- Read and write operations\n"
            "- Directory management\n"
            "- Multi-cluster file support\n"
            "\n"
            "Enjoy exploring the file system!\n";

        size_t content_len = strlen(content);
        size_t written = fat32_write(fd, content, content_len);

        logger("   Wrote %zu of %zu bytes\n", written, content_len);

        // 验证写入
        fat32_lseek(fd, 0, FAT32_SEEK_SET);
        char *read_buffer = (char *)kalloc_pages((content_len + PAGE_SIZE) / PAGE_SIZE);
        if (read_buffer != NULL) {
            size_t read_bytes = fat32_read(fd, read_buffer, content_len + 100);
            read_buffer[read_bytes] = '\0';

            logger("   Read back %zu bytes\n", read_bytes);
            if (read_bytes == written && memcmp(content, read_buffer, written) == 0) {
                logger("   Data verification: PASSED\n");
            } else {
                logger("   Data verification: FAILED\n");
            }

            kfree_pages(read_buffer, (content_len + PAGE_SIZE) / PAGE_SIZE);
        }

        fat32_close(fd);
    }

    // 演示2：创建二进制文件
    logger("2. Creating binary data file...\n");
    fd = fat32_open("/binary.dat");
    if (fd > 0) {
        // 创建1KB的二进制测试数据
        const uint32_t binary_size = 1024;
        uint8_t *binary_data = (uint8_t *)kalloc_pages((binary_size + PAGE_SIZE - 1) / PAGE_SIZE);
        if (binary_data != NULL) {
            // 填充递增的字节模式
            for (uint32_t i = 0; i < binary_size; i++) {
                binary_data[i] = (uint8_t)(i & 0xFF);
            }

            size_t written = fat32_write(fd, binary_data, binary_size);
            logger("   Wrote %zu bytes of binary data\n", written);

            // 验证二进制数据
            fat32_lseek(fd, 0, FAT32_SEEK_SET);
            uint8_t *verify_data = (uint8_t *)kalloc_pages((binary_size + PAGE_SIZE - 1) / PAGE_SIZE);
            if (verify_data != NULL) {
                size_t read_bytes = fat32_read(fd, verify_data, binary_size);

                uint8_t verification_passed = 1;
                for (uint32_t i = 0; i < read_bytes; i++) {
                    if (verify_data[i] != (uint8_t)(i & 0xFF)) {
                        verification_passed = 0;
                        break;
                    }
                }

                if (verification_passed && read_bytes == written) {
                    logger("   Binary data verification: PASSED\n");
                } else {
                    logger("   Binary data verification: FAILED\n");
                }

                kfree_pages(verify_data, (binary_size + PAGE_SIZE - 1) / PAGE_SIZE);
            }

            kfree_pages(binary_data, (binary_size + PAGE_SIZE - 1) / PAGE_SIZE);
        }

        fat32_close(fd);
    }

    // 演示3：文件修改和覆盖
    logger("3. Testing file modification...\n");
    fd = fat32_open("/modify.txt");
    if (fd > 0) {
        // 写入初始内容
        const char *original = "Original content that will be modified";
        fat32_write(fd, original, strlen(original));

        // 回到文件开头并覆盖部分内容
        fat32_lseek(fd, 0, FAT32_SEEK_SET);
        const char *modified = "MODIFIED";
        size_t mod_written = fat32_write(fd, modified, strlen(modified));

        logger("   Modified %zu bytes at file beginning\n", mod_written);

        // 验证修改结果
        fat32_lseek(fd, 0, FAT32_SEEK_SET);
        char result_buffer[100];
        size_t result_read = fat32_read(fd, result_buffer, sizeof(result_buffer) - 1);
        result_buffer[result_read] = '\0';

        logger("   Final content: '%s'\n", result_buffer);

        fat32_close(fd);
    }

    // 显示最终的文件列表
    logger("4. Final file listing...\n");
    fat32_dir_entry_t entries[10];
    uint32_t count;
    fat32_error_t result = fat32_listdir("/", entries, 10, &count);
    if (result == FAT32_OK) {
        logger("   Root directory contains %u files:\n", count);
        for (uint32_t i = 0; i < count; i++) {
            char filename[13];
            fat32_dir_convert_from_short_name(entries[i].name, filename, sizeof(filename));
            char size_str[20];
            fat32_utils_format_file_size(entries[i].file_size, size_str, sizeof(size_str));
            logger("   - %s (%s)\n", filename, size_str);
        }
    }

    logger("=== Write Operations Demo Completed ===\n");
}
