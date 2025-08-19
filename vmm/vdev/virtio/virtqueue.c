/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file virtqueue.c
 * @brief Implementation of virtqueue.c
 * @author Avatar Project Team
 * @date 2024
 */

#include "vmm/virtio.h"
#include "vmm/vgic.h"
#include "io.h"
#include "lib/avatar_string.h"

// 外部声明
extern virtio_device_t _virtio_devices[];
extern uint32_t        _virtio_device_count;

int
virtio_queue_init(virtqueue_t *vq, uint32_t num)
{
    if (!vq || num == 0 || num > 256) {
        return -1;
    }

    vq->num            = num;
    vq->ready          = 0;
    vq->last_avail_idx = 0;
    vq->used_idx       = 0;
    vq->desc_addr      = 0;
    vq->avail_addr     = 0;
    vq->used_addr      = 0;
    vq->desc           = NULL;
    vq->avail          = NULL;
    vq->used           = NULL;

    return 0;
}

void
virtio_queue_reset(virtqueue_t *vq)
{
    if (!vq)
        return;

    vq->num            = 0;
    vq->ready          = 0;
    vq->last_avail_idx = 0;
    vq->used_idx       = 0;
    vq->desc_addr      = 0;
    vq->avail_addr     = 0;
    vq->used_addr      = 0;
    vq->desc           = NULL;
    vq->avail          = NULL;
    vq->used           = NULL;
}

bool
virtio_queue_available(virtqueue_t *vq)
{
    if (!vq || !vq->ready || !vq->avail) {
        return false;
    }

    return vq->last_avail_idx != vq->avail->idx;
}

int
virtio_queue_pop(virtqueue_t *vq, virtq_desc_t *desc_chain, uint32_t *desc_count)
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
    uint32_t count    = 0;

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

void
virtio_queue_push(virtqueue_t *vq, uint32_t desc_idx, uint32_t len)
{
    if (!vq || !vq->ready || !vq->used) {
        return;
    }

    // 添加到已用环
    uint16_t used_idx            = vq->used_idx % vq->num;
    vq->used->ring[used_idx].id  = desc_idx;
    vq->used->ring[used_idx].len = len;

    // 内存屏障确保写入顺序
    __asm__ volatile("dmb sy" ::: "memory");

    vq->used_idx++;
    vq->used->idx = vq->used_idx;

    // 再次内存屏障
    __asm__ volatile("dmb sy" ::: "memory");
}

void
virtio_queue_notify(virtio_device_t *dev, uint32_t queue_idx)
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

void
virtio_inject_irq(virtio_device_t *dev, uint32_t int_type)
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
        extern void vgic_inject_spi(tcb_t * task, int32_t irq_id);
        vgic_inject_spi(task, dev->irq);
    }
}
