#include "vmm/virtio.h"
#include "vmm/vgic.h"
#include "io.h"
#include "lib/avatar_string.h"

// 外部声明
extern virtio_device_t _virtio_devices[];
extern uint32_t _virtio_device_count;

int virtio_queue_init(virtqueue_t *vq, uint32_t num)
{
    if (!vq || num == 0 || num > 256) {
        return -1;
    }
    
    vq->num = num;
    vq->ready = 0;
    vq->last_avail_idx = 0;
    vq->used_idx = 0;
    vq->desc_addr = 0;
    vq->avail_addr = 0;
    vq->used_addr = 0;
    vq->desc = NULL;
    vq->avail = NULL;
    vq->used = NULL;
    
    return 0;
}

void virtio_queue_reset(virtqueue_t *vq)
{
    if (!vq) return;
    
    vq->num = 0;
    vq->ready = 0;
    vq->last_avail_idx = 0;
    vq->used_idx = 0;
    vq->desc_addr = 0;
    vq->avail_addr = 0;
    vq->used_addr = 0;
    vq->desc = NULL;
    vq->avail = NULL;
    vq->used = NULL;
}

bool virtio_queue_available(virtqueue_t *vq)
{
    if (!vq || !vq->ready || !vq->avail) {
        return false;
    }
    
    return vq->last_avail_idx != vq->avail->idx;
}

int virtio_queue_pop(virtqueue_t *vq, virtq_desc_t *desc_chain, uint32_t *desc_count)
{
    if (!vq || !vq->ready || !vq->avail || !vq->desc || !desc_chain || !desc_count) {
        return -1;
    }
    
    if (vq->last_avail_idx == vq->avail->idx) {
        return -1;  // 没有可用的描述符
    }
    
    // 获取下一个可用的描述符索引
    uint16_t desc_idx = vq->avail->ring[vq->last_avail_idx % vq->num];
    uint16_t head_idx = desc_idx;
    uint32_t count = 0;
    
    // 遍历描述符链
    do {
        if (desc_idx >= vq->num) {
            logger_error("Invalid descriptor index: %d\n", desc_idx);
            return -1;
        }
        
        if (count >= 16) {  // 限制描述符链长度
            logger_error("Descriptor chain too long\n");
            return -1;
        }
        
        // 复制描述符
        desc_chain[count] = vq->desc[desc_idx];
        count++;
        
        // 检查是否有下一个描述符
        if (vq->desc[desc_idx].flags & VIRTQ_DESC_F_NEXT) {
            desc_idx = vq->desc[desc_idx].next;
        } else {
            break;
        }
    } while (true);
    
    *desc_count = count;
    vq->last_avail_idx++;
    
    return head_idx;
}

void virtio_queue_push(virtqueue_t *vq, uint32_t desc_idx, uint32_t len)
{
    if (!vq || !vq->ready || !vq->used) {
        return;
    }
    
    // 添加到已用环
    uint16_t used_idx = vq->used_idx % vq->num;
    vq->used->ring[used_idx].id = desc_idx;
    vq->used->ring[used_idx].len = len;
    
    // 内存屏障确保写入顺序
    __asm__ volatile("dmb sy" ::: "memory");
    
    vq->used_idx++;
    vq->used->idx = vq->used_idx;
    
    // 再次内存屏障
    __asm__ volatile("dmb sy" ::: "memory");
}

void virtio_queue_notify(virtio_device_t *dev, uint32_t queue_idx)
{
    if (!dev || queue_idx >= dev->num_queues) {
        return;
    }
    
    virtqueue_t *vq = &dev->queues[queue_idx];
    if (!vq->ready) {
        return;
    }
    
    // 检查是否需要发送中断
    bool need_interrupt = true;
    
    if (vq->used && (vq->used->flags & VIRTQ_USED_F_NO_NOTIFY)) {
        need_interrupt = false;
    }
    
    if (need_interrupt) {
        virtio_inject_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
}

void virtio_inject_irq(virtio_device_t *dev, uint32_t int_type)
{
    if (!dev || !dev->vm) {
        return;
    }

    dev->interrupt_status |= int_type;

    // 向VM注入中断 - 使用SPI中断
    // 需要找到当前VM的任务
    tcb_t *task = curr_task_el2();

    if (task && task->curr_vm == dev->vm) {
        // 使用现有的vgic_inject_spi函数
        extern void vgic_inject_spi(tcb_t *task, int32_t irq_id);
        vgic_inject_spi(task, dev->irq);
    }
}

bool virtio_has_feature(virtio_device_t *dev, uint32_t feature)
{
    if (!dev) return false;
    
    return (dev->driver_features & (1ULL << feature)) != 0;
}

void virtio_set_features(virtio_device_t *dev, uint64_t features)
{
    if (!dev) return;
    
    // 只允许设置设备支持的特性
    dev->driver_features = features & dev->device_features;
}



// VirtIO MMIO 陷入处理函数
bool handle_virtio_mmio_access(uint64_t addr, uint32_t *value, bool is_write, uint32_t size)
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

// 调试函数：打印VirtIO设备状态
void virtio_debug_device(virtio_device_t *dev)
{
    if (!dev) return;
    
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
                   i, vq->num, vq->ready, vq->last_avail_idx, vq->used_idx);
    }
    logger_info("===========================\n");
}


