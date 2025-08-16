#include "virtio_block_frontend.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "timer.h"

// 全局 VirtIO Block 设备
static virtio_blk_device_t g_virtio_block_device;
static bool g_virtio_block_initialized = false;

/**
 * 初始化 VirtIO Block 前端驱动
 * 这个函数在 Avatar 系统启动时调用
 */
int avatar_virtio_block_init(void)
{
    logger_info("Initializing Avatar VirtIO Block frontend...\n");
    
    // 初始化 VirtIO 前端子系统
    if (virtio_blk_frontend_init() < 0) {
        logger_error("Failed to initialize VirtIO frontend subsystem\n");
        return -1;
    }
    
    // 扫描 VirtIO Block 设备
    uint64_t block_device_addr = scan_for_virtio_block_device(VIRTIO_ID_BLOCK);
    if (block_device_addr == 0) {
        logger_warn("No VirtIO block device found\n");
        return -1;
    }
    
    logger_info("Found VirtIO block device at 0x%lx\n", block_device_addr);
    
    // 初始化块设备
    if (virtio_blk_init(&g_virtio_block_device, block_device_addr, 0) < 0) {
        logger_error("Failed to initialize VirtIO block device\n");
        return -1;
    }
    
    // 打印设备信息
    virtio_blk_print_info(&g_virtio_block_device);
    
    g_virtio_block_initialized = true;
    logger_info("Avatar VirtIO Block frontend initialized successfully\n");
    
    return 0;
}

/**
 * 获取 VirtIO Block 设备
 */
virtio_blk_device_t *avatar_get_virtio_block_device(void)
{
    if (!g_virtio_block_initialized) {
        logger_error("VirtIO Block device not initialized\n");
        return NULL;
    }
    
    return &g_virtio_block_device;
}

/**
 * 从 VirtIO Block 设备读取数据
 * 这个函数可以被 Avatar VMM 后端调用，将数据传递给 Guest
 */
int avatar_virtio_block_read(uint64_t sector, void *buffer, uint32_t sector_count)
{
    if (!g_virtio_block_initialized) {
        logger_error("VirtIO Block device not initialized\n");
        return -1;
    }
    
    if (!buffer || sector_count == 0) {
        logger_error("Invalid parameters for block read\n");
        return -1;
    }
    
    logger_debug("Reading %u sectors from sector %llu\n", sector_count, sector);
    
    // 逐个扇区读取（可以优化为批量读取）
    for (uint32_t i = 0; i < sector_count; i++) {
        uint8_t *sector_buffer = (uint8_t*)buffer + (i * 512);
        
        if (virtio_blk_read_sector(&g_virtio_block_device, sector + i, 
                                  sector_buffer, 1) < 0) {
            logger_error("Failed to read sector %llu\n", sector + i);
            return -1;
        }
    }
    
    logger_debug("Successfully read %u sectors\n", sector_count);
    return 0;
}

/**
 * 向 VirtIO Block 设备写入数据
 * 这个函数可以被 Avatar VMM 后端调用，将 Guest 的数据写入存储
 */
int avatar_virtio_block_write(uint64_t sector, const void *buffer, uint32_t sector_count)
{
    if (!g_virtio_block_initialized) {
        logger_error("VirtIO Block device not initialized\n");
        return -1;
    }
    
    if (!buffer || sector_count == 0) {
        logger_error("Invalid parameters for block write\n");
        return -1;
    }
    
    logger_debug("Writing %u sectors to sector %llu\n", sector_count, sector);
    
    // 逐个扇区写入（可以优化为批量写入）
    for (uint32_t i = 0; i < sector_count; i++) {
        const uint8_t *sector_buffer = (const uint8_t*)buffer + (i * 512);
        
        if (virtio_blk_write_sector(&g_virtio_block_device, sector + i,
                                   sector_buffer, 1) < 0) {
            logger_error("Failed to write sector %llu\n", sector + i);
            return -1;
        }
    }
    
    logger_debug("Successfully wrote %u sectors\n", sector_count);
    return 0;
}

/**
 * 获取 VirtIO Block 设备信息
 */
int avatar_virtio_block_get_info(uint64_t *capacity, uint32_t *block_size)
{
    if (!g_virtio_block_initialized) {
        logger_error("VirtIO Block device not initialized\n");
        return -1;
    }
    
    if (capacity) {
        *capacity = g_virtio_block_device.capacity;
    }
    
    if (block_size) {
        *block_size = g_virtio_block_device.block_size;
    }
    
    return 0;
}

/**
 * 简单的读写测试
 */
int avatar_virtio_block_test(void)
{
    logger_info("=== Avatar VirtIO Block Test ===\n");
    
    if (!g_virtio_block_initialized) {
        logger_error("VirtIO Block device not initialized\n");
        return -1;
    }
    
    // 测试数据
    uint8_t write_buffer[512];
    uint8_t read_buffer[512];
    
    // 生成测试模式
    for (uint32_t i = 0; i < 512; i++) {
        write_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // 选择安全的测试扇区
    uint64_t test_sector = g_virtio_block_device.capacity - 10;
    
    logger_info("Testing sector %llu\n", test_sector);
    
    // 写入测试
    logger_info("Writing test data...\n");
    if (avatar_virtio_block_write(test_sector, write_buffer, 1) < 0) {
        logger_error("Write test failed\n");
        return -1;
    }
    
    // 读取测试
    logger_info("Reading test data...\n");
    memset(read_buffer, 0, sizeof(read_buffer));
    if (avatar_virtio_block_read(test_sector, read_buffer, 1) < 0) {
        logger_error("Read test failed\n");
        return -1;
    }
    
    // 验证数据
    logger_info("Verifying test data...\n");
    for (uint32_t i = 0; i < 512; i++) {
        if (read_buffer[i] != write_buffer[i]) {
            logger_error("Data mismatch at offset %u: expected 0x%02x, got 0x%02x\n",
                        i, write_buffer[i], read_buffer[i]);
            return -1;
        }
    }
    
    logger_info("VirtIO Block test PASSED!\n");
    return 0;
}

/**
 * 与 VMM 后端集成的接口函数
 * 这些函数可以被你的 VMM VirtIO 后端调用
 */

// 为 VMM 后端提供的读取接口
int vmm_backend_read_from_host_storage(uint64_t sector, void *buffer, uint32_t count)
{
    return avatar_virtio_block_read(sector, buffer, count);
}

// 为 VMM 后端提供的写入接口
int vmm_backend_write_to_host_storage(uint64_t sector, const void *buffer, uint32_t count)
{
    return avatar_virtio_block_write(sector, buffer, count);
}

// 为 VMM 后端提供的容量查询接口
int vmm_backend_get_storage_info(uint64_t *total_sectors, uint32_t *sector_size)
{
    return avatar_virtio_block_get_info(total_sectors, sector_size);
}

/**
 * 调试和监控函数
 */
void avatar_virtio_block_print_status(void)
{
    logger_info("=== Avatar VirtIO Block Status ===\n");
    
    if (!g_virtio_block_initialized) {
        logger_info("Status: Not initialized\n");
        return;
    }
    
    logger_info("Status: Initialized\n");
    logger_info("Base address: 0x%lx\n", g_virtio_block_device.dev->base_addr);
    logger_info("Capacity: %llu sectors\n", g_virtio_block_device.capacity);
    logger_info("Block size: %u bytes\n", g_virtio_block_device.block_size);
    logger_info("Total size: %llu bytes\n", 
                g_virtio_block_device.capacity * g_virtio_block_device.block_size);
    
    logger_info("================================\n");
}

/**
 * 在主程序中的使用示例
 */
void avatar_main_virtio_block_example(void)
{
    logger_info("Starting Avatar VirtIO Block example...\n");
    
    // 1. 初始化 VirtIO Block 前端
    if (avatar_virtio_block_init() < 0) {
        logger_error("Failed to initialize VirtIO Block\n");
        return;
    }
    
    // 2. 打印状态信息
    avatar_virtio_block_print_status();
    
    // 3. 运行测试
    if (avatar_virtio_block_test() < 0) {
        logger_error("VirtIO Block test failed\n");
        return;
    }
    
    // 4. 现在可以在 VMM 后端中使用这些接口
    logger_info("VirtIO Block frontend ready for VMM backend integration\n");
    
    // 示例：读取第一个扇区
    uint8_t sector_0[512];
    if (vmm_backend_read_from_host_storage(0, sector_0, 1) == 0) {
        logger_info("Successfully read sector 0 from host storage\n");
        // 这个数据现在可以通过 VMM 后端传递给 Guest
    }
}
