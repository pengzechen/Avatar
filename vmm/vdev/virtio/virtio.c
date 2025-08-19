/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file virtio.c
 * @brief Implementation of virtio.c
 * @author Avatar Project Team
 * @date 2024
 */

#include "vmm/virtio.h"
#include "vmm/vm.h"
#include "vmm/vgic.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "os_cfg.h"

// VirtIO 设备池
static virtio_device_t _virtio_devices[VM_NUM_MAX * 8];  // 每个VM最多8个VirtIO设备
static uint32_t        _virtio_device_count = 0;

// VirtQueue 静态池
static virtqueue_t _virtqueue_pool[VM_NUM_MAX * 8 * 8];  // 每个设备最多8个队列
static uint32_t    _virtqueue_pool_used = 0;

// VirtIO Magic Number
#define VIRTIO_MMIO_MAGIC 0x74726976  // "virt"

// 清空结构
void
virtio_global_init(void)
{
    memset(_virtio_devices, 0, sizeof(_virtio_devices));
    memset(_virtqueue_pool, 0, sizeof(_virtqueue_pool));
    _virtio_device_count = 0;
    _virtqueue_pool_used = 0;
    logger_info("VirtIO subsystem initialized\n");
}

// 被对应的virtio设备create调用
virtio_device_t *
virtio_create_device(uint32_t device_id, uint64_t base_addr, uint32_t irq)
{
    if (_virtio_device_count >= (VM_NUM_MAX * 8)) {
        logger_error("No more VirtIO devices can be allocated\n");
        return NULL;
    }

    virtio_device_t *dev = &_virtio_devices[_virtio_device_count++];
    memset(dev, 0, sizeof(virtio_device_t));

    dev->device_id         = device_id;
    dev->vendor_id         = 0x554D4551;  // "QEMU" 反向
    dev->version           = 2;           // VirtIO 1.0
    dev->base_addr         = base_addr;
    dev->irq               = irq;
    dev->status            = 0;
    dev->config_generation = 1;

    // 根据设备类型设置默认特性
    switch (device_id) {
        case VIRTIO_ID_NET:
            dev->device_features = (1ULL << 0) | (1ULL << 5);  // CSUM + MAC
            dev->num_queues      = 2;                          // RX + TX
            break;
        case VIRTIO_ID_BLOCK:
            dev->device_features = (1ULL << 0) | (1ULL << 1);  // BARRIER + SIZE_MAX
            dev->num_queues      = 1;
            break;
        case VIRTIO_ID_CONSOLE:
            dev->device_features = (1ULL << 0);  // SIZE
            dev->num_queues      = 2;            // RX + TX
            break;
        default:
            dev->device_features = 0;
            dev->num_queues      = 1;
            break;
    }

    // 分配队列 - 使用静态池
    if (dev->num_queues > 0 && dev->num_queues <= 8) {
        if (_virtqueue_pool_used + dev->num_queues <= (VM_NUM_MAX * 8 * 8)) {
            dev->queues = &_virtqueue_pool[_virtqueue_pool_used];
            _virtqueue_pool_used += dev->num_queues;
            memset(dev->queues, 0, sizeof(virtqueue_t) * dev->num_queues);
        } else {
            logger_error("VirtIO queue pool exhausted\n");
            return NULL;
        }
    }

    logger_info("Created VirtIO device: ID=%d, base=0x%llx, irq=%d\n", device_id, base_addr, irq);
    return dev;
}

// 被对应的virtio设备destroy调用
void
virtio_destroy_device(virtio_device_t *dev)
{
    if (!dev)
        return;

    if (dev->queues) {
        for (uint32_t i = 0; i < dev->num_queues; i++) {
            virtio_queue_reset(&dev->queues[i]);
        }
        // 不需要释放静态分配的队列
        dev->queues = NULL;
    }

    // config 通常指向设备特定结构的成员，不需要释放
    dev->config = NULL;

    memset(dev, 0, sizeof(virtio_device_t));
}

// virtio 读事件
bool
virtio_mmio_read(virtio_device_t *dev, uint64_t offset, uint32_t *value, uint32_t size)
{
    if (!dev || !value)
        return false;

    virtqueue_t *vq = NULL;
    if (dev->queue_sel < dev->num_queues) {
        vq = &dev->queues[dev->queue_sel];
    }

    switch (offset) {
        case VIRTIO_MMIO_MAGIC_VALUE:
            *value = VIRTIO_MMIO_MAGIC;
            break;

        case VIRTIO_MMIO_VERSION:
            *value = dev->version;
            break;

        case VIRTIO_MMIO_DEVICE_ID:
            *value = dev->device_id;
            break;

        case VIRTIO_MMIO_VENDOR_ID:
            *value = dev->vendor_id;
            break;

        case VIRTIO_MMIO_DEVICE_FEATURES:
            if (dev->features_sel == 0) {
                *value = (uint32_t) (dev->device_features & 0xFFFFFFFF);
            } else if (dev->features_sel == 1) {
                *value = (uint32_t) (dev->device_features >> 32);
            } else {
                *value = 0;
            }
            break;

        case VIRTIO_MMIO_QUEUE_NUM_MAX:
            *value = vq ? 256 : 0;  // 最大队列大小
            break;

        case VIRTIO_MMIO_QUEUE_READY:
            *value = vq ? vq->ready : 0;
            break;

        case VIRTIO_MMIO_INTERRUPT_STATUS:
            *value = dev->interrupt_status;
            break;

        case VIRTIO_MMIO_STATUS:
            *value = dev->status;
            break;

        case VIRTIO_MMIO_CONFIG_GENERATION:
            *value = dev->config_generation;
            break;

        default:
            if (offset >= VIRTIO_MMIO_CONFIG && dev->config) {
                uint32_t config_offset = offset - VIRTIO_MMIO_CONFIG;
                if (config_offset + size <= dev->config_len) {
                    memcpy(value, (uint8_t *) dev->config + config_offset, size);
                    return true;
                }
            }
            *value = 0;
            break;
    }

    return true;
}

// virtio 写事件
bool
virtio_mmio_write(virtio_device_t *dev, uint64_t offset, uint32_t value, uint32_t size)
{
    if (!dev)
        return false;

    virtqueue_t *vq = NULL;
    if (dev->queue_sel < dev->num_queues) {
        vq = &dev->queues[dev->queue_sel];
    }

    switch (offset) {
        case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
            dev->features_sel = value;
            break;

        case VIRTIO_MMIO_DRIVER_FEATURES:
            if (dev->features_sel == 0) {
                dev->driver_features = (dev->driver_features & 0xFFFFFFFF00000000ULL) | value;
            } else if (dev->features_sel == 1) {
                dev->driver_features =
                    (dev->driver_features & 0x00000000FFFFFFFFULL) | ((uint64_t) value << 32);
            }
            break;

        case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
            dev->features_sel = value;
            break;

        case VIRTIO_MMIO_QUEUE_SEL:
            dev->queue_sel = value;
            break;

        case VIRTIO_MMIO_QUEUE_NUM:
            if (vq && value <= 256) {
                virtio_queue_init(vq, value);
            }
            break;

        case VIRTIO_MMIO_QUEUE_READY:
            if (vq) {
                vq->ready = value ? 1 : 0;
            }
            break;

        case VIRTIO_MMIO_QUEUE_NOTIFY:
            if (value < dev->num_queues && dev->queue_notify) {
                dev->queue_notify(dev, value);
            }
            break;

        case VIRTIO_MMIO_INTERRUPT_ACK:
            dev->interrupt_status &= ~value;
            break;

        case VIRTIO_MMIO_STATUS:
            dev->status = value;
            if (value == 0 && dev->reset) {
                dev->reset(dev);
            }
            break;

        case VIRTIO_MMIO_QUEUE_DESC_LOW:
            if (vq) {
                vq->desc_addr = (vq->desc_addr & 0xFFFFFFFF00000000ULL) | value;
            }
            break;

        case VIRTIO_MMIO_QUEUE_DESC_HIGH:
            if (vq) {
                vq->desc_addr = (vq->desc_addr & 0x00000000FFFFFFFFULL) | ((uint64_t) value << 32);
                // 映射描述符表
                vq->desc = (virtq_desc_t *) vq->desc_addr;
            }
            break;

        case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
            if (vq) {
                vq->avail_addr = (vq->avail_addr & 0xFFFFFFFF00000000ULL) | value;
            }
            break;

        case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
            if (vq) {
                vq->avail_addr =
                    (vq->avail_addr & 0x00000000FFFFFFFFULL) | ((uint64_t) value << 32);
                // 映射可用环
                vq->avail = (virtq_avail_t *) vq->avail_addr;
            }
            break;

        case VIRTIO_MMIO_QUEUE_USED_LOW:
            if (vq) {
                vq->used_addr = (vq->used_addr & 0xFFFFFFFF00000000ULL) | value;
            }
            break;

        case VIRTIO_MMIO_QUEUE_USED_HIGH:
            if (vq) {
                vq->used_addr = (vq->used_addr & 0x00000000FFFFFFFFULL) | ((uint64_t) value << 32);
                // 映射已用环
                vq->used = (virtq_used_t *) vq->used_addr;
            }
            break;

        default:
            if (offset >= VIRTIO_MMIO_CONFIG && dev->config) {
                uint32_t config_offset = offset - VIRTIO_MMIO_CONFIG;
                if (config_offset + size <= dev->config_len) {
                    memcpy((uint8_t *) dev->config + config_offset, &value, size);
                    return true;
                }
            }
            break;
    }

    return true;
}

// VirtIO MMIO 陷入处理函数
bool
handle_virtio_mmio_access(uint64_t addr, uint32_t *value, bool is_write, uint32_t size)
{
    virtio_device_t *dev = virtio_find_device_by_addr(addr);
    if (!dev) {
        return false;
    }

    uint64_t offset = addr - dev->base_addr;

    if (is_write) {
        return virtio_mmio_write(dev, offset, *value, size);
    } else {
        return virtio_mmio_read(dev, offset, value, size);
    }
}

// VirtIO 设备查找函数
virtio_device_t *
virtio_find_device_by_addr(uint64_t addr)
{
    for (uint32_t i = 0; i < _virtio_device_count; i++) {
        virtio_device_t *dev = &_virtio_devices[i];
        if (dev->base_addr <= addr && addr < (dev->base_addr + 0x1000)) {
            return dev;
        }
    }
    return NULL;
}

// 调试函数：打印VirtIO设备状态
void
virtio_debug_device(virtio_device_t *dev)
{
    if (!dev)
        return;

    logger_info("=== VirtIO Device Debug ===\n");
    logger_info("Device ID: %d\n", dev->device_id);
    logger_info("Vendor ID: 0x%x\n", dev->vendor_id);
    logger_info("Base Address: 0x%llx\n", dev->base_addr);
    logger_info("IRQ: %d\n", dev->irq);
    logger_info("Status: 0x%x\n", dev->status);
    logger_info("Device Features: 0x%llx\n", dev->device_features);
    logger_info("Driver Features: 0x%llx\n", dev->driver_features);
    logger_info("Interrupt Status: 0x%x\n", dev->interrupt_status);
    logger_info("Number of Queues: %d\n", dev->num_queues);

    for (uint32_t i = 0; i < dev->num_queues; i++) {
        virtqueue_t *vq = &dev->queues[i];
        logger_info("Queue %d: num=%d, ready=%d, last_avail=%d, used=%d\n",
                    i,
                    vq->num,
                    vq->ready,
                    vq->last_avail_idx,
                    vq->used_idx);
    }
    logger_info("===========================\n");
}