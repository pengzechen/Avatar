/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file virtio_block.c
 * @brief Implementation of virtio_block.c
 * @author Avatar Project Team
 * @date 2024
 */

#include "virtio_block_frontend.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "timer.h"

// 全局 VirtIO Block 设备
static virtio_blk_device_t g_virtio_block_device;
static bool                g_virtio_block_initialized = false;

/**
 * 初始化 VirtIO Block 前端驱动
 * 这个函数在 Avatar 系统启动时调用
 */
int
avatar_virtio_block_init(void)
{
    if (g_virtio_block_initialized) {
        logger_warn("VirtIO Block already initialized\n");
        return 0;
    }

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

    logger_virtio_front_debug("Found VirtIO block device at 0x%lx\n", block_device_addr);

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
virtio_blk_device_t *
avatar_get_virtio_block_device(void)
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
int
avatar_virtio_block_read(uint64_t sector, void *buffer, uint32_t sector_count)
{
    if (!g_virtio_block_initialized) {
        logger_error("VirtIO Block device not initialized\n");
        return -1;
    }

    if (!buffer || sector_count == 0) {
        logger_error("Invalid parameters for block read\n");
        return -1;
    }

    logger_virtio_front_debug("Reading %u sectors from sector %llu\n", sector_count, sector);

    // 移除复杂的连续读取检测，直接优化单扇区读取
    // 对于单扇区读取，直接返回，不使用批量处理（减少开销）
    if (sector_count == 1) {
        return virtio_blk_read_sector(&g_virtio_block_device, sector, buffer, 1);
    }

    // 对于多扇区读取，使用原有的批量处理
    const uint32_t MAX_BATCH_SECTORS = 128;

    // 优化：使用批量读取
    uint32_t remaining_sectors = sector_count;
    uint64_t current_sector    = sector;
    uint8_t *current_buffer    = (uint8_t *) buffer;

    while (remaining_sectors > 0) {
        uint32_t batch_size =
            (remaining_sectors > MAX_BATCH_SECTORS) ? MAX_BATCH_SECTORS : remaining_sectors;

        if (virtio_blk_read_sector(&g_virtio_block_device,
                                   current_sector,
                                   current_buffer,
                                   batch_size) < 0) {
            logger_error("Failed to read batch starting at sector %llu, count %u\n",
                         current_sector,
                         batch_size);
            return -1;
        }

        remaining_sectors -= batch_size;
        current_sector += batch_size;
        current_buffer += batch_size * 512;
    }

    logger_virtio_front_debug("Successfully read %u sectors\n", sector_count);
    return 0;
}

/**
 * 向 VirtIO Block 设备写入数据
 * 这个函数可以被 Avatar VMM 后端调用，将 Guest 的数据写入存储
 */
int
avatar_virtio_block_write(uint64_t sector, const void *buffer, uint32_t sector_count)
{
    if (!g_virtio_block_initialized) {
        logger_error("VirtIO Block device not initialized\n");
        return -1;
    }

    if (!buffer || sector_count == 0) {
        logger_error("Invalid parameters for block write\n");
        return -1;
    }

    logger_virtio_front_debug("Writing %u sectors to sector %llu\n", sector_count, sector);

    // 优化：使用批量写入而不是逐个扇区写入
    const uint32_t MAX_BATCH_SECTORS = 128;  // 64KB per batch - 合理的批量大小
    uint32_t       remaining_sectors = sector_count;
    uint64_t       current_sector    = sector;
    const uint8_t *current_buffer    = (const uint8_t *) buffer;

    while (remaining_sectors > 0) {
        uint32_t batch_size =
            (remaining_sectors > MAX_BATCH_SECTORS) ? MAX_BATCH_SECTORS : remaining_sectors;

        if (virtio_blk_write_sector(&g_virtio_block_device,
                                    current_sector,
                                    current_buffer,
                                    batch_size) < 0) {
            logger_error("Failed to write batch starting at sector %llu, count %u\n",
                         current_sector,
                         batch_size);
            return -1;
        }

        remaining_sectors -= batch_size;
        current_sector += batch_size;
        current_buffer += batch_size * 512;
    }

    logger_virtio_front_debug("Successfully wrote %u sectors\n", sector_count);
    return 0;
}


/**
 * 与 VMM 后端集成的接口函数
 * 这些函数可以被你的 VMM VirtIO 后端调用
 */

// 为 VMM 后端提供的读取接口
int
vmm_backend_read_from_host_storage(uint64_t sector, void *buffer, uint32_t count)
{
    return avatar_virtio_block_read(sector, buffer, count);
}

// 为 VMM 后端提供的写入接口
int
vmm_backend_write_to_host_storage(uint64_t sector, const void *buffer, uint32_t count)
{
    return avatar_virtio_block_write(sector, buffer, count);
}

// 为 VMM 后端提供的容量查询接口
int
vmm_backend_get_storage_info(uint64_t *total_sectors, uint32_t *sector_size)
{
    return avatar_virtio_block_get_info(total_sectors, sector_size);
}


/**
 * 调试和监控函数
 */
void
avatar_virtio_block_print_status(void)
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
 * 获取 VirtIO Block 设备信息
 */
int
avatar_virtio_block_get_info(uint64_t *capacity, uint32_t *block_size)
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
