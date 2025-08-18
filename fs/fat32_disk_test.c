/**
 * @file fat32_disk_test.c
 * @brief FAT32磁盘模块测试程序
 * 
 * 测试重构后的FAT32磁盘模块，验证VirtIO块设备和内存模拟的功能
 */

#include "fs/fat32_disk.h"
#include "io.h"

/**
 * @brief 测试磁盘初始化和基本信息
 */
void test_disk_init(void)
{
    logger("=== Testing Disk Initialization ===\n");
    
    fat32_disk_t *disk = fat32_get_disk();
    
    // 测试初始化
    fat32_error_t result = fat32_disk_init(disk);
    if (result == FAT32_OK) {
        logger("✓ Disk initialization successful\n");
        
        // 打印磁盘信息
        fat32_disk_print_info(disk);
        
        // 检查是否使用VirtIO
        if (fat32_disk_is_using_virtio()) {
            logger("✓ Using VirtIO block device\n");
        } else {
            logger("✓ Using memory simulation\n");
        }
        
    } else {
        logger("✗ Disk initialization failed: %d\n", result);
    }
}

/**
 * @brief 测试磁盘读写功能
 */
void test_disk_rw(void)
{
    logger("=== Testing Disk Read/Write ===\n");
    
    fat32_disk_t *disk = fat32_get_disk();
    
    if (!disk->initialized) {
        logger("✗ Disk not initialized\n");
        return;
    }
    
    // 执行读写测试
    fat32_error_t result = fat32_disk_test_rw(disk);
    if (result == FAT32_OK) {
        logger("✓ Disk read/write test passed\n");
    } else {
        logger("✗ Disk read/write test failed: %d\n", result);
    }
}

/**
 * @brief 测试磁盘格式化
 */
void test_disk_format(void)
{
    logger("=== Testing Disk Format ===\n");
    
    fat32_disk_t *disk = fat32_get_disk();
    
    if (!disk->initialized) {
        logger("✗ Disk not initialized\n");
        return;
    }
    
    // 测试格式化
    fat32_error_t result = fat32_disk_format(disk, "AVATAR");
    if (result == FAT32_OK) {
        logger("✓ Disk format successful\n");
        
        // 打印格式化后的信息
        fat32_disk_print_info(disk);
        
    } else {
        logger("✗ Disk format failed: %d\n", result);
    }
}

/**
 * @brief 测试设备信息获取
 */
void test_device_info(void)
{
    logger("=== Testing Device Info ===\n");
    
    fat32_disk_t *disk = fat32_get_disk();
    
    if (!disk->initialized) {
        logger("✗ Disk not initialized\n");
        return;
    }
    
    uint64_t capacity;
    uint32_t block_size;
    
    fat32_error_t result = fat32_disk_get_device_info(disk, &capacity, &block_size);
    if (result == FAT32_OK) {
        logger("✓ Device info retrieved successfully\n");
        logger("  Capacity: %llu sectors\n", capacity);
        logger("  Block Size: %u bytes\n", block_size);
        logger("  Total Size: %llu MB\n", (capacity * block_size) / (1024 * 1024));
    } else {
        logger("✗ Failed to get device info: %d\n", result);
    }
}

/**
 * @brief 测试磁盘统计信息
 */
void test_disk_stats(void)
{
    logger("=== Testing Disk Statistics ===\n");
    
    fat32_disk_t *disk = fat32_get_disk();
    
    if (!disk->initialized) {
        logger("✗ Disk not initialized\n");
        return;
    }
    
    uint32_t read_count, write_count, error_count;
    
    fat32_error_t result = fat32_disk_get_stats(disk, &read_count, &write_count, &error_count);
    if (result == FAT32_OK) {
        logger("✓ Disk statistics retrieved\n");
        logger("  Read Operations: %u\n", read_count);
        logger("  Write Operations: %u\n", write_count);
        logger("  Error Count: %u\n", error_count);
    } else {
        logger("✗ Failed to get disk statistics: %d\n", result);
    }
}

/**
 * @brief 测试磁盘清理
 */
void test_disk_cleanup(void)
{
    logger("=== Testing Disk Cleanup ===\n");
    
    fat32_disk_t *disk = fat32_get_disk();
    
    fat32_error_t result = fat32_disk_cleanup(disk);
    if (result == FAT32_OK) {
        logger("✓ Disk cleanup successful\n");
    } else {
        logger("✗ Disk cleanup failed: %d\n", result);
    }
}

/**
 * @brief 主测试函数
 */
void fat32_disk_run_tests(void)
{
    logger("========================================\n");
    logger("FAT32 Disk Module Test Suite\n");
    logger("========================================\n");
    
    // 测试序列
    test_disk_init();
    test_device_info();
    test_disk_rw();
    test_disk_stats();
    test_disk_format();
    test_disk_stats();  // 再次检查统计信息
    test_disk_cleanup();
    
    logger("========================================\n");
    logger("FAT32 Disk Module Tests Completed\n");
    logger("========================================\n");
}

/**
 * @brief 简化的测试入口
 */
void fat32_disk_quick_test(void)
{
    logger("=== FAT32 Disk Quick Test ===\n");
    
    fat32_disk_t *disk = fat32_get_disk();
    
    // 初始化
    if (fat32_disk_init(disk) != FAT32_OK) {
        logger("✗ Quick test failed: initialization\n");
        return;
    }
    
    // 读写测试
    if (fat32_disk_test_rw(disk) != FAT32_OK) {
        logger("✗ Quick test failed: read/write\n");
        fat32_disk_cleanup(disk);
        return;
    }
    
    // 清理
    fat32_disk_cleanup(disk);
    
    logger("✓ Quick test passed\n");
}
