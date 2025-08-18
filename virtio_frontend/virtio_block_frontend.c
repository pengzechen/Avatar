#include "virtio_block_frontend.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "timer.h"
#include "mmio.h"
#include "mem/barrier.h"
#include "mem/kallocator.h"

// Device memory management - allocate per device as needed
#define MAX_VIRTIO_DEVICES        16
#define VIRTIO_DEVICE_MEMORY_SIZE 0x80000  // 512KB per device
#define VIRTIO_QUEUE_DESC_OFFSET  0x0      // Descriptor table at start (0x1000 aligned)
#define VIRTIO_QUEUE_AVAIL_OFFSET 0x100    // Available ring at desc + 0x100
#define VIRTIO_QUEUE_USED_OFFSET  0xf00    // Used ring at avail + 0x1000

static void *g_device_memory[MAX_VIRTIO_DEVICES] = {0};

// Device memory management functions
static uint64_t
virtio_get_device_base_addr(uint32_t device_index)
{
    // Check device index bounds
    if (device_index >= MAX_VIRTIO_DEVICES) {
        logger_error("Device index %u exceeds maximum devices (%u)\n",
                     device_index,
                     MAX_VIRTIO_DEVICES);
        return 0;
    }

    // Allocate device memory if not already allocated
    if (g_device_memory[device_index] == NULL) {
        // Allocate memory for this specific device
        g_device_memory[device_index] = kalloc(VIRTIO_DEVICE_MEMORY_SIZE, 0x1000);  // 4KB aligned

        if (g_device_memory[device_index] == NULL) {
            logger_error("Failed to allocate memory for VirtIO device %u\n", device_index);
            return 0;
        }

        logger_info("VirtIO device %u memory allocated: 0x%lx (%u KB)\n",
                    device_index,
                    (uint64_t) g_device_memory[device_index],
                    VIRTIO_DEVICE_MEMORY_SIZE / 1024);
    }

    return (uint64_t) g_device_memory[device_index];
}

static void
virtio_free_device_memory(uint32_t device_index)
{
    if (device_index >= MAX_VIRTIO_DEVICES) {
        logger_error("Device index %u exceeds maximum devices (%u)\n",
                     device_index,
                     MAX_VIRTIO_DEVICES);
        return;
    }

    if (g_device_memory[device_index] != NULL) {
        kfree(g_device_memory[device_index]);
        g_device_memory[device_index] = NULL;

        logger_info("VirtIO device %u memory freed\n", device_index);
    }
}

static uint64_t
virtio_get_queue_desc_addr(uint32_t device_index, uint32_t queue_id)
{
    uint64_t device_base = virtio_get_device_base_addr(device_index);
    if (device_base == 0) {
        return 0;
    }

    // Each queue gets 0x4000 (16KB) within the device memory
    // Descriptor table is at the start of queue memory (0x1000 aligned)
    uint64_t queue_base = device_base + (queue_id * 0x4000);
    uint64_t desc_addr  = (queue_base + 0xFFF) & ~0xFFF;  // Ensure 0x1000 alignment

    logger_debug("Device %u Queue %u desc addr: 0x%lx\n", device_index, queue_id, desc_addr);
    return desc_addr;
}

static uint64_t
virtio_get_queue_avail_addr(uint32_t device_index, uint32_t queue_id)
{
    uint64_t desc_addr = virtio_get_queue_desc_addr(device_index, queue_id);
    if (desc_addr == 0) {
        return 0;
    }

    // Available ring is at desc + 0x100
    uint64_t avail_addr = desc_addr + VIRTIO_QUEUE_AVAIL_OFFSET;

    logger_debug("Device %u Queue %u avail addr: 0x%lx\n", device_index, queue_id, avail_addr);
    return avail_addr;
}

static uint64_t
virtio_get_queue_used_addr(uint32_t device_index, uint32_t queue_id)
{
    uint64_t avail_addr = virtio_get_queue_avail_addr(device_index, queue_id);
    if (avail_addr == 0) {
        return 0;
    }

    // Used ring is at avail + 0x1000
    uint64_t used_addr = avail_addr + VIRTIO_QUEUE_USED_OFFSET;

    logger_debug("Device %u Queue %u used addr: 0x%lx\n", device_index, queue_id, used_addr);
    return used_addr;
}

// 初始化 VirtIO 前端子系统
int
virtio_blk_frontend_init(void)
{
    logger_info("VirtIO Block frontend initialized\n");
    return 0;
}

// VirtIO MMIO 设备初始化
int
virtio_mmio_init(virtio_device_t *dev, uint64_t base_addr, uint32_t device_index)
{
    dev->base_addr    = base_addr;
    dev->device_index = device_index;
    dev->num_queues   = 0;

    // 检查 Magic Value
    uint32_t magic = virtio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC) {
        logger_error("Invalid VirtIO magic value: 0x%x\n", magic);
        return -1;
    }

    // 获取版本
    dev->version = virtio_read32(dev, VIRTIO_MMIO_VERSION);
    logger_info("VirtIO version: %d\n", dev->version);

    if (dev->version < 1 || dev->version > 2) {
        logger_error("Unsupported VirtIO version: %d\n", dev->version);
        return -1;
    }

    // 获取设备和厂商 ID
    dev->device_id = virtio_read32(dev, VIRTIO_MMIO_DEVICE_ID);
    dev->vendor_id = virtio_read32(dev, VIRTIO_MMIO_VENDOR_ID);

    logger_info("VirtIO device ID: %d, vendor ID: 0x%x\n", dev->device_id, dev->vendor_id);

    // 重置设备
    virtio_write32(dev, VIRTIO_MMIO_STATUS, 0);

    // 确认设备
    virtio_set_status(dev, VIRTIO_STATUS_ACKNOWLEDGE);

    // 设置驱动状态
    virtio_set_status(dev, VIRTIO_STATUS_DRIVER);

    return 0;
}

// 队列设置
int
virtio_queue_setup(virtio_device_t *dev, uint32_t queue_id, uint32_t queue_size)
{
    if (queue_id >= 16) {
        logger_error("Queue ID %d too large\n", queue_id);
        return -1;
    }

    virtio_queue_t *queue = &dev->queues[queue_id];
    queue->queue_id       = queue_id;
    queue->queue_size     = queue_size;
    queue->last_used_idx  = 0;
    queue->free_head      = 0;
    queue->num_free       = queue_size;

    // 选择队列
    virtio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, queue_id);

    // 检查队列是否存在
    uint32_t max_size = virtio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_size == 0) {
        logger_error("Queue %d does not exist\n", queue_id);
        return -1;
    }

    if (queue_size > max_size) {
        queue_size        = max_size;
        queue->queue_size = queue_size;
    }

    // 设置队列大小
    virtio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, queue_size);

    // 获取队列内存地址
    queue->desc_addr  = virtio_get_queue_desc_addr(dev->device_index, queue_id);
    queue->avail_addr = virtio_get_queue_avail_addr(dev->device_index, queue_id);
    queue->used_addr  = virtio_get_queue_used_addr(dev->device_index, queue_id);

    if (queue->desc_addr == 0 || queue->avail_addr == 0 || queue->used_addr == 0) {
        logger_error("Failed to get queue memory addresses\n");
        return -1;
    }

    // 设置指针
    queue->desc  = (virtq_desc_t *) queue->desc_addr;
    queue->avail = (virtq_avail_t *) queue->avail_addr;
    queue->used  = (virtq_used_t *) queue->used_addr;

    logger_info("Queue %d memory layout:\n", queue_id);
    logger_info("  Desc:  0x%lx\n", queue->desc_addr);
    logger_info("  Avail: 0x%lx\n", queue->avail_addr);
    logger_info("  Used:  0x%lx\n", queue->used_addr);

    // 初始化描述符表
    for (uint32_t i = 0; i < queue_size; i++) {
        queue->desc[i].addr  = 0;
        queue->desc[i].len   = 0;
        queue->desc[i].flags = 0;
        queue->desc[i].next  = (i + 1) % queue_size;
    }

    // 初始化可用环
    queue->avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;  // 不使用中断
    queue->avail->idx   = 0;

    // 初始化已用环
    queue->used->flags = 0;
    queue->used->idx   = 0;

    // 配置队列地址
    if (dev->version >= 2) {
        // 现代模式
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, queue->desc_addr & 0xFFFFFFFF);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, queue->desc_addr >> 32);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_AVAIL_LOW, queue->avail_addr & 0xFFFFFFFF);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, queue->avail_addr >> 32);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_USED_LOW, queue->used_addr & 0xFFFFFFFF);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_USED_HIGH, queue->used_addr >> 32);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    } else {
        // 传统模式
        virtio_write32(dev, VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_ALIGN, 4096);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_PFN, queue->desc_addr >> 12);
    }

    dev->num_queues = queue_id + 1;

    logger_warn("Queue %d setup complete: size=%d, desc=0x%lx, avail=0x%lx, used=0x%lx\n",
                queue_id,
                queue_size,
                queue->desc_addr,
                queue->avail_addr,
                queue->used_addr);
    return 0;
}

// 添加缓冲区到队列
int
virtio_queue_add_buf(virtio_device_t *dev,
                     uint32_t         queue_id,
                     uint64_t        *buffers,
                     uint32_t        *lengths,
                     uint32_t         out_num,
                     uint32_t         in_num)
{
    if (queue_id >= dev->num_queues) {
        logger_error("Invalid queue ID: %d\n", queue_id);
        return -1;
    }

    virtio_queue_t *queue      = &dev->queues[queue_id];
    uint32_t        total_desc = out_num + in_num;

    if (total_desc == 0 || total_desc > queue->num_free) {
        logger_error("Not enough free descriptors: need %d, have %d\n",
                     total_desc,
                     queue->num_free);
        return -1;
    }

    uint16_t head = queue->free_head;
    uint16_t prev = head;

    // 设置输出描述符
    for (uint32_t i = 0; i < out_num; i++) {
        queue->desc[prev].addr  = buffers[i];
        queue->desc[prev].len   = lengths[i];
        queue->desc[prev].flags = (i + 1 < total_desc) ? VIRTQ_DESC_F_NEXT : 0;
        if (i + 1 < total_desc) {
            prev = queue->desc[prev].next;
        }
    }

    // 设置输入描述符
    for (uint32_t i = 0; i < in_num; i++) {
        queue->desc[prev].addr  = buffers[out_num + i];
        queue->desc[prev].len   = lengths[out_num + i];
        queue->desc[prev].flags = VIRTQ_DESC_F_WRITE;
        if (i + 1 < in_num) {
            queue->desc[prev].flags |= VIRTQ_DESC_F_NEXT;
            prev = queue->desc[prev].next;
        }
    }

    // 更新空闲列表
    queue->free_head = queue->desc[prev].next;
    queue->num_free -= total_desc;

    // 添加到可用环
    uint16_t avail_idx                                = queue->avail->idx;
    queue->avail->ring[avail_idx % queue->queue_size] = head;

    // 内存屏障后更新索引
    dsb(st);  // 写屏障
    queue->avail->idx = avail_idx + 1;

    return head;
}

// 通知设备处理队列
int
virtio_queue_kick(virtio_device_t *dev, uint32_t queue_id)
{
    if (queue_id >= dev->num_queues) {
        logger_error("Invalid queue ID: %d\n", queue_id);
        return -1;
    }

    // 通知设备
    virtio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, queue_id);
    return 0;
}

// 从队列获取已完成的缓冲区
int
virtio_queue_get_buf(virtio_device_t *dev, uint32_t queue_id, uint32_t *len)
{
    if (queue_id >= dev->num_queues) {
        logger_error("Invalid queue ID: %d\n", queue_id);
        return -1;
    }

    virtio_queue_t *queue = &dev->queues[queue_id];

    // 检查是否有已用缓冲区
    dsb(ld);  // 读屏障
    if (queue->last_used_idx == queue->used->idx) {
        return -1;  // 没有已用缓冲区
    }

    // 获取已用缓冲区
    virtq_used_elem_t *used_elem = &queue->used->ring[queue->last_used_idx % queue->queue_size];
    uint32_t           desc_id   = used_elem->id;
    if (len) {
        *len = used_elem->len;
    }

    // 释放描述符
    uint16_t desc_idx   = desc_id;
    uint32_t desc_count = 0;

    while (true) {
        uint16_t next     = queue->desc[desc_idx].next;
        bool     has_next = queue->desc[desc_idx].flags & VIRTQ_DESC_F_NEXT;

        // 清除描述符
        queue->desc[desc_idx].addr  = 0;
        queue->desc[desc_idx].len   = 0;
        queue->desc[desc_idx].flags = 0;
        queue->desc[desc_idx].next  = queue->free_head;

        queue->free_head = desc_idx;
        desc_count++;

        if (!has_next) {
            break;
        }
        desc_idx = next;
    }

    queue->num_free += desc_count;
    queue->last_used_idx++;

    return desc_id;
}

// 获取 VirtIO Block 设备配置
void
virtio_blk_get_config(virtio_blk_device_t *blk_dev)
{
    virtio_device_t *dev = blk_dev->dev;

    // 从设备读取配置
    uint64_t config_base = dev->base_addr + VIRTIO_MMIO_CONFIG;

    blk_dev->config.capacity = virtio_read64(dev, VIRTIO_MMIO_CONFIG);
    blk_dev->config.size_max = virtio_read32(dev, VIRTIO_MMIO_CONFIG + 8);
    blk_dev->config.seg_max  = virtio_read32(dev, VIRTIO_MMIO_CONFIG + 12);
    blk_dev->config.blk_size = virtio_read32(dev, VIRTIO_MMIO_CONFIG + 20);

    // 设置默认值
    blk_dev->block_size = blk_dev->config.blk_size ? blk_dev->config.blk_size : 512;
    blk_dev->capacity   = blk_dev->config.capacity;

    logger_info("Block device config: capacity=%llu, block_size=%u\n",
                blk_dev->capacity,
                blk_dev->block_size);
}

// 初始化 VirtIO Block 设备
int
virtio_blk_init(virtio_blk_device_t *blk_dev, uint64_t base_addr, uint32_t device_index)
{
    // 分配设备结构
    virtio_device_t *dev = kalloc(sizeof(virtio_device_t), 8);
    if (!dev) {
        logger_error("Failed to allocate device structure\n");
        return -1;
    }
    blk_dev->dev = dev;

    // 初始化 VirtIO 设备
    if (virtio_mmio_init(dev, base_addr, device_index) < 0) {
        logger_error("Failed to initialize VirtIO device\n");
        return -1;
    }

    // 检查是否为块设备
    if (dev->device_id != VIRTIO_ID_BLOCK) {
        logger_error("Device is not a block device (ID: %d)\n", dev->device_id);
        return -1;
    }

    // 读取设备特性
    virtio_write32(dev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    dev->device_features = virtio_read32(dev, VIRTIO_MMIO_DEVICE_FEATURES);

    logger_info("Block device features: 0x%lx\n", dev->device_features);

    // 设置驱动特性（基本操作不需要特殊特性）
    dev->driver_features = 0;
    virtio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    virtio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, dev->driver_features);

    // 设置特性 OK
    virtio_set_status(dev, VIRTIO_STATUS_FEATURES_OK);

    // 检查设备是否接受我们的特性
    if (!(virtio_get_status(dev) & VIRTIO_STATUS_FEATURES_OK)) {
        logger_error("Device rejected our features\n");
        return -1;
    }

    // 设置队列（块设备通常使用队列 0）
    if (virtio_queue_setup(dev, 0, 16) < 0) {
        logger_error("Failed to setup queue\n");
        return -1;
    }

    // 设置驱动 OK
    virtio_set_status(dev, VIRTIO_STATUS_DRIVER_OK);

    // 读取设备配置
    virtio_blk_get_config(blk_dev);

    logger_info("VirtIO block device initialized successfully\n");
    return 0;
}

// 从设备读取扇区
int
virtio_blk_read_sector(virtio_blk_device_t *blk_dev, uint64_t sector, void *buffer, uint32_t count)
{
    if (!blk_dev || !blk_dev->dev || !buffer) {
        logger_error("Invalid parameters\n");
        return -1;
    }

    uint32_t sector_size = blk_dev->block_size;
    uint32_t total_size  = count * sector_size;

    // 分配请求结构
    virtio_blk_req_t *req    = (virtio_blk_req_t *) kalloc(sizeof(virtio_blk_req_t), 8);
    uint8_t          *status = (uint8_t *) kalloc(1, 8);

    if (!req || !status) {
        logger_error("Failed to allocate request structures\n");
        return -1;
    }

    // 设置请求
    req->type     = VIRTIO_BLK_T_IN;  // 读操作
    req->reserved = 0;
    req->sector   = sector;

    // 设置缓冲区数组
    uint64_t buffers[3];
    uint32_t lengths[3];

    buffers[0] = (uint64_t) req;
    lengths[0] = sizeof(virtio_blk_req_t);

    buffers[1] = (uint64_t) buffer;
    lengths[1] = total_size;

    buffers[2] = (uint64_t) status;
    lengths[2] = 1;

    logger_debug("Reading sector %llu, count %u, total_size %u\n", sector, count, total_size);

    // 添加缓冲区到队列（1 个输出，2 个输入）
    int desc_id = virtio_queue_add_buf(blk_dev->dev, 0, buffers, lengths, 1, 2);
    if (desc_id < 0) {
        logger_error("Failed to add buffer to queue\n");
        kfree(req);
        kfree(status);
        return -1;
    }

    // 通知队列
    if (virtio_queue_kick(blk_dev->dev, 0) < 0) {
        logger_error("Failed to kick queue\n");
        kfree(req);
        kfree(status);
        return -1;
    }

    // 轮询完成（无中断模式）
    uint32_t timeout = 1000000;  // 大超时值
    uint32_t len;
    int      result = -1;

    while (timeout-- > 0) {
        result = virtio_queue_get_buf(blk_dev->dev, 0, &len);
        if (result >= 0) {
            break;
        }
        // 小延迟
        for (volatile int i = 0; i < 100; i++)
            ;
    }

    if (result < 0) {
        logger_error("Timeout waiting for block read completion\n");
        kfree(req);
        kfree(status);
        return -1;
    }

    // 检查状态
    if (*status != VIRTIO_BLK_S_OK) {
        logger_error("Block read failed with status: %d\n", *status);
        kfree(req);
        kfree(status);
        return -1;
    }

    logger_debug("Block read completed successfully, len=%u\n", len);

    kfree(req);
    kfree(status);
    return 0;
}

// 向设备写入扇区
int
virtio_blk_write_sector(virtio_blk_device_t *blk_dev,
                        uint64_t             sector,
                        const void          *buffer,
                        uint32_t             count)
{
    if (!blk_dev || !blk_dev->dev || !buffer) {
        logger_error("Invalid parameters\n");
        return -1;
    }

    uint32_t sector_size = blk_dev->block_size;
    uint32_t total_size  = count * sector_size;

    // 分配请求结构
    virtio_blk_req_t *req    = (virtio_blk_req_t *) kalloc(sizeof(virtio_blk_req_t), 8);
    uint8_t          *status = (uint8_t *) kalloc(1, 8);

    if (!req || !status) {
        logger_error("Failed to allocate request structures\n");
        return -1;
    }

    // 设置请求
    req->type     = VIRTIO_BLK_T_OUT;  // 写操作
    req->reserved = 0;
    req->sector   = sector;

    // 设置缓冲区数组
    uint64_t buffers[3];
    uint32_t lengths[3];

    buffers[0] = (uint64_t) req;
    lengths[0] = sizeof(virtio_blk_req_t);

    buffers[1] = (uint64_t) buffer;
    lengths[1] = total_size;

    buffers[2] = (uint64_t) status;
    lengths[2] = 1;

    logger_debug("Writing sector %llu, count %u, total_size %u\n", sector, count, total_size);

    // 添加缓冲区到队列（2 个输出，1 个输入）
    int desc_id = virtio_queue_add_buf(blk_dev->dev, 0, buffers, lengths, 2, 1);
    if (desc_id < 0) {
        logger_error("Failed to add buffer to queue\n");
        kfree(req);
        kfree(status);
        return -1;
    }

    // 通知队列
    if (virtio_queue_kick(blk_dev->dev, 0) < 0) {
        logger_error("Failed to kick queue\n");
        kfree(req);
        kfree(status);
        return -1;
    }

    // 轮询完成（无中断模式）
    uint32_t timeout = 1000000;  // 大超时值
    uint32_t len;
    int      result = -1;

    while (timeout-- > 0) {
        result = virtio_queue_get_buf(blk_dev->dev, 0, &len);
        if (result >= 0) {
            break;
        }
        // 小延迟
        for (volatile int i = 0; i < 10; i++)
            ;
    }

    if (result < 0) {
        logger_error("Timeout waiting for block write completion\n");
        kfree(req);
        kfree(status);
        return -1;
    }

    // 检查状态
    if (*status != VIRTIO_BLK_S_OK) {
        logger_error("Block write failed with status: %d\n", *status);
        kfree(req);
        kfree(status);
        return -1;
    }

    logger_debug("Block write completed successfully, len=%u\n", len);

    kfree(req);
    kfree(status);
    return 0;
}


// 扫描 VirtIO Block 设备
uint64_t
scan_for_virtio_block_device(uint32_t found_device_id)
{
    logger_info("Scanning for VirtIO devices that match ID %d...\n", found_device_id);

    for (uint32_t i = 0; i < VIRTIO_SCAN_COUNT; i++) {
        uint64_t addr = VIRTIO_SCAN_BASE_ADDR + (i * VIRTIO_SCAN_STEP);

        // Check magic value first
        uint32_t magic = mmio_read32((volatile void *) (addr + VIRTIO_MMIO_MAGIC_VALUE));
        if (magic != VIRTIO_MMIO_MAGIC) {
            logger_debug("Address 0x%lx: Invalid magic 0x%x\n", addr, magic);
            continue;
        }

        // Check version
        uint32_t version = mmio_read32((volatile void *) (addr + VIRTIO_MMIO_VERSION));
        if (version < 1 || version > 2) {
            logger_debug("Address 0x%lx: Invalid version %u\n", addr, version);
            continue;
        }

        // Check device ID
        uint32_t device_id = mmio_read32((volatile void *) (addr + VIRTIO_MMIO_DEVICE_ID));
        uint32_t vendor_id = mmio_read32((volatile void *) (addr + VIRTIO_MMIO_VENDOR_ID));

        logger_debug("VirtIO device at 0x%lx: ID=%u, Vendor=0x%x, Version=%u\n",
                     addr,
                     device_id,
                     vendor_id,
                     version);

        if (device_id == found_device_id) {
            logger_info("Found VirtIO %d device at address 0x%lx!\n", found_device_id, addr);
            return addr;
        }
    }

    logger_error("No VirtIO %d device found in scan range\n", found_device_id);
    return 0;
}

// 打印设备信息
void
virtio_blk_print_info(virtio_blk_device_t *blk_dev)
{
    if (!blk_dev || !blk_dev->dev) {
        logger_error("Invalid block device\n");
        return;
    }

    logger_info("=== VirtIO Block Device Info ===\n");
    logger_info("Base address: 0x%lx\n", blk_dev->dev->base_addr);
    logger_info("Device ID: %d\n", blk_dev->dev->device_id);
    logger_info("Vendor ID: 0x%x\n", blk_dev->dev->vendor_id);
    logger_info("Version: %d\n", blk_dev->dev->version);
    logger_info("Device features: 0x%lx\n", blk_dev->dev->device_features);
    logger_info("Driver features: 0x%lx\n", blk_dev->dev->driver_features);
    logger_info("Capacity: %llu sectors\n", blk_dev->capacity);
    logger_info("Block size: %u bytes\n", blk_dev->block_size);
    logger_info("Total size: %llu bytes\n", blk_dev->capacity * blk_dev->block_size);
    logger_info("==============================\n");
}

// 简单的 VirtIO 设备检测函数
void
virtio_detect_devices(void)
{
    logger_info("=== VirtIO Device Detection ===\n");

    for (uint32_t i = 0; i < VIRTIO_SCAN_COUNT; i++) {
        uint64_t addr = VIRTIO_SCAN_BASE_ADDR + (i * VIRTIO_SCAN_STEP);

        // 检查 Magic Value
        uint32_t magic = mmio_read32((volatile void *) (addr + VIRTIO_MMIO_MAGIC_VALUE));
        if (magic != VIRTIO_MMIO_MAGIC) {
            continue;  // 跳过无效设备
        }

        // 读取设备信息
        uint32_t version   = mmio_read32((volatile void *) (addr + VIRTIO_MMIO_VERSION));
        uint32_t device_id = mmio_read32((volatile void *) (addr + VIRTIO_MMIO_DEVICE_ID));
        uint32_t vendor_id = mmio_read32((volatile void *) (addr + VIRTIO_MMIO_VENDOR_ID));

        logger_info("VirtIO device found at 0x%lx:\n", addr);
        logger_info("  Magic: 0x%x\n", magic);
        logger_info("  Version: %d\n", version);
        logger_info("  Device ID: %d", device_id);

        // 解释设备类型
        switch (device_id) {
            case 1:
                logger_info(" (Network)\n");
                break;
            case 2:
                logger_info(" (Block)\n");
                break;
            case 3:
                logger_info(" (Console)\n");
                break;
            case 4:
                logger_info(" (RNG)\n");
                break;
            default:
                logger_info(" (Unknown)\n");
                break;
        }

        logger_info("  Vendor ID: 0x%x\n", vendor_id);
        logger_info("\n");
    }

    logger_info("=== VirtIO Detection Complete ===\n");
}
