#include "virtio_block_frontend.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "timer.h"
#include "mem/kallocator.h"

// 测试数据
static uint8_t test_write_buffer[512];
static uint8_t test_read_buffer[512];

// 生成测试数据模式
static void generate_test_pattern(uint8_t *buffer, uint32_t size, uint32_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)((seed + i) & 0xFF);
    }
}

// 验证测试数据模式
static bool verify_test_pattern(const uint8_t *buffer, uint32_t size, uint32_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)((seed + i) & 0xFF);
        if (buffer[i] != expected) {
            logger_error("Data mismatch at offset %u: expected 0x%02x, got 0x%02x\n",
                        i, expected, buffer[i]);
            return false;
        }
    }
    return true;
}

// 打印缓冲区内容（十六进制）
static void print_hex_dump(const uint8_t *buffer, uint32_t size, const char *title)
{
    logger_info("%s:\n", title);
    for (uint32_t i = 0; i < size && i < 64; i += 16) {
        logger_info("%04x: ", i);
        for (uint32_t j = 0; j < 16 && (i + j) < size; j++) {
            logger_info("%02x ", buffer[i + j]);
        }
        logger_info("\n");
    }
    if (size > 64) {
        logger_info("... (showing first 64 bytes of %u)\n", size);
    }
}

// 基本读取测试
int virtio_blk_test_basic_read(virtio_blk_device_t *blk_dev)
{
    logger_info("=== Basic Read Test ===\n");
    
    // 清空读缓冲区
    memset(test_read_buffer, 0, sizeof(test_read_buffer));
    
    logger_info("Reading first sector (sector 0)...\n");
    
    // 读取第一个扇区
    if (virtio_blk_read_sector(blk_dev, 0, test_read_buffer, 1) < 0) {
        logger_error("Failed to read first sector\n");
        return -1;
    }
    
    logger_info("Successfully read first sector!\n");
    
    // 打印前 64 字节
    print_hex_dump(test_read_buffer, 64, "First 64 bytes of sector 0");
    
    return 0;
}

// 读写测试
int virtio_blk_test_read_write(virtio_blk_device_t *blk_dev)
{
    logger_info("=== Read/Write Test ===\n");
    
    // 选择一个安全的测试扇区（避免破坏重要数据）
    uint64_t test_sector = blk_dev->capacity - 10;  // 倒数第10个扇区
    uint32_t test_seed = 0x12345678;
    
    logger_info("Testing sector %llu\n", test_sector);
    
    // 生成测试数据
    generate_test_pattern(test_write_buffer, sizeof(test_write_buffer), test_seed);
    
    logger_info("Writing test pattern...\n");
    
    // 写入测试数据
    if (virtio_blk_write_sector(blk_dev, test_sector, test_write_buffer, 1) < 0) {
        logger_error("Failed to write test sector\n");
        return -1;
    }
    
    logger_info("Write completed, now reading back...\n");
    
    // 清空读缓冲区
    memset(test_read_buffer, 0, sizeof(test_read_buffer));
    
    // 读取数据
    if (virtio_blk_read_sector(blk_dev, test_sector, test_read_buffer, 1) < 0) {
        logger_error("Failed to read test sector\n");
        return -1;
    }
    
    logger_info("Read completed, verifying data...\n");
    
    // 验证数据
    if (!verify_test_pattern(test_read_buffer, sizeof(test_read_buffer), test_seed)) {
        logger_error("Data verification failed!\n");
        print_hex_dump(test_write_buffer, 32, "Expected data (first 32 bytes)");
        print_hex_dump(test_read_buffer, 32, "Actual data (first 32 bytes)");
        return -1;
    }
    
    logger_info("Read/Write test PASSED!\n");
    return 0;
}

// 性能测试
int virtio_blk_test_performance(virtio_blk_device_t *blk_dev)
{
    logger_info("=== Performance Test ===\n");
    
    const uint32_t test_sectors = 20;
    const uint32_t iterations = 5;
    uint64_t test_start_sector = blk_dev->capacity - 20;
    
    uint8_t *test_buffer = kalloc(test_sectors * 512, 16);
    if (!test_buffer) {
        logger_error("Failed to allocate test buffer\n");
        return -1;
    }
    
    // 生成测试数据
    generate_test_pattern(test_buffer, test_sectors * 512, 0xABCDEF00);
    
    uint64_t total_write_time = 0;
    uint64_t total_read_time = 0;
    
    logger_info("Testing %u sectors x %u iterations\n", test_sectors, iterations);
    
    for (uint32_t i = 0; i < iterations; i++) {
        // 写入测试
        uint64_t start_time = read_cntpct_el0();
        
        for (uint32_t j = 0; j < test_sectors; j++) {
            if (virtio_blk_write_sector(blk_dev, test_start_sector + j, 
                                       test_buffer + j * 512, 1) < 0) {
                logger_error("Write failed in iteration %u, sector %u\n", i, j);
                kfree(test_buffer);
                return -1;
            }
        }
        
        uint64_t write_time = read_cntpct_el0() - start_time;
        total_write_time += write_time;
        
        // 读取测试
        start_time = read_cntpct_el0();
        
        for (uint32_t j = 0; j < test_sectors; j++) {
            if (virtio_blk_read_sector(blk_dev, test_start_sector + j,
                                      test_buffer + j * 512, 1) < 0) {
                logger_error("Read failed in iteration %u, sector %u\n", i, j);
                kfree(test_buffer);
                return -1;
            }
        }
        
        uint64_t read_time = read_cntpct_el0() - start_time;
        total_read_time += read_time;
        
        logger_info("Iteration %u: Write=%llu ticks, Read=%llu ticks\n", 
                   i + 1, write_time, read_time);
    }
    
    // 计算平均性能
    uint64_t avg_write_time = total_write_time / iterations;
    uint64_t avg_read_time = total_read_time / iterations;
    
    logger_info("Performance test results:\n");
    logger_info("  Average write time: %llu ticks (%u sectors)\n", avg_write_time, test_sectors);
    logger_info("  Average read time: %llu ticks (%u sectors)\n", avg_read_time, test_sectors);
    
    kfree(test_buffer);
    logger_info("Performance test completed\n");
    
    return 0;
}

// 主测试函数
int virtio_block_test(void)
{
    logger_info("Starting VirtIO Block device test\n");

    // VirtIO now uses kallocator directly - no separate initialization needed
    if (!kallocator_is_initialized()) {
        logger_error("Kernel allocator not initialized\n");
        return -1;
    }
    
    // 初始化前端子系统
    if (virtio_blk_frontend_init() < 0) {
        logger_error("Failed to initialize VirtIO frontend\n");
        return -1;
    }
    
    // 扫描 VirtIO Block 设备
    uint64_t block_device_addr = scan_for_virtio_block_device(VIRTIO_ID_BLOCK);
    if (block_device_addr == 0) {
        logger_error("Failed to find VirtIO block device\n");
        return -1;
    }
    
    // 初始化块设备
    virtio_blk_device_t blk_dev;
    
    if (virtio_blk_init(&blk_dev, block_device_addr, 0) < 0) {
        logger_error("Failed to initialize VirtIO block device\n");
        return -1;
    }
    
    // 打印设备信息
    virtio_blk_print_info(&blk_dev);
    
    int test_results = 0;
    
    // 运行基本读取测试
    if (virtio_blk_test_basic_read(&blk_dev) < 0) {
        logger_error("Basic read test failed\n");
        test_results = -1;
    }
    
    // 运行读写测试
    if (virtio_blk_test_read_write(&blk_dev) < 0) {
        logger_error("Read/write test failed\n");
        test_results = -1;
    }
    
    // 运行性能测试
    if (virtio_blk_test_performance(&blk_dev) < 0) {
        logger_error("Performance test failed\n");
        test_results = -1;
    }
    
    if (test_results == 0) {
        logger_info("=== All VirtIO Block tests PASSED ===\n");
    } else {
        logger_error("=== Some VirtIO Block tests FAILED ===\n");
    }

    kallocator_info();
    
    return test_results;
}

// 简单的设备信息查询
void virtio_block_query_device(void)
{
    logger_info("=== VirtIO Block Device Query ===\n");
    
    uint64_t block_device_addr = scan_for_virtio_block_device(VIRTIO_ID_BLOCK);
    if (block_device_addr == 0) {
        logger_info("No VirtIO block device found\n");
        return;
    }
    
    logger_info("VirtIO block device found at 0x%lx\n", block_device_addr);
    
    // 简单读取设备信息
    volatile uint32_t *base = (volatile uint32_t*)block_device_addr;
    uint32_t magic = base[VIRTIO_MMIO_MAGIC_VALUE / 4];
    uint32_t version = base[VIRTIO_MMIO_VERSION / 4];
    uint32_t device_id = base[VIRTIO_MMIO_DEVICE_ID / 4];
    uint32_t vendor_id = base[VIRTIO_MMIO_VENDOR_ID / 4];
    
    logger_info("Magic: 0x%x\n", magic);
    logger_info("Version: %d\n", version);
    logger_info("Device ID: %d\n", device_id);
    logger_info("Vendor ID: 0x%x\n", vendor_id);
}
