#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include "avatar_types.h"
#include "vmm/vm.h"

// VirtIO 设备类型定义
#define VIRTIO_ID_NET       1
#define VIRTIO_ID_BLOCK     2
#define VIRTIO_ID_CONSOLE   3
#define VIRTIO_ID_RNG       4
#define VIRTIO_ID_BALLOON   5
#define VIRTIO_ID_SCSI      8
#define VIRTIO_ID_9P        9
#define VIRTIO_ID_GPU       16

// VirtIO 设备状态
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET    64
#define VIRTIO_STATUS_FAILED        128

// VirtIO 配置空间偏移
#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW     0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH    0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW      0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH     0x0a4
#define VIRTIO_MMIO_CONFIG_GENERATION   0x0fc
#define VIRTIO_MMIO_CONFIG              0x100

// VirtIO 中断类型
#define VIRTIO_MMIO_INT_VRING           (1 << 0)
#define VIRTIO_MMIO_INT_CONFIG          (1 << 1)

// VirtQueue 描述符标志
#define VIRTQ_DESC_F_NEXT       1
#define VIRTQ_DESC_F_WRITE      2
#define VIRTQ_DESC_F_INDIRECT   4

// VirtQueue 可用环标志
#define VIRTQ_AVAIL_F_NO_INTERRUPT  1

// VirtQueue 已用环标志
#define VIRTQ_USED_F_NO_NOTIFY      1

// VirtQueue 描述符
typedef struct virtq_desc {
    uint64_t addr;      // 缓冲区物理地址
    uint32_t len;       // 缓冲区长度
    uint16_t flags;     // 标志位
    uint16_t next;      // 下一个描述符索引
} virtq_desc_t;

// VirtQueue 可用环
typedef struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];    // 可变长度数组
    // uint16_t used_event; // 在 ring[num] 位置
} virtq_avail_t;

// VirtQueue 已用环元素
typedef struct virtq_used_elem {
    uint32_t id;        // 描述符链头索引
    uint32_t len;       // 写入的字节数
} virtq_used_elem_t;

// VirtQueue 已用环
typedef struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[];   // 可变长度数组
    // uint16_t avail_event;    // 在 ring[num] 位置
} virtq_used_t;

// VirtQueue 结构
typedef struct virtqueue {
    uint32_t num;               // 队列大小
    uint32_t ready;             // 队列是否就绪
    
    // 队列内存布局
    virtq_desc_t *desc;         // 描述符表
    virtq_avail_t *avail;       // 可用环
    virtq_used_t *used;         // 已用环
    
    // 队列状态
    uint16_t last_avail_idx;    // 上次处理的可用索引
    uint16_t used_idx;          // 已用索引
    
    // 物理地址
    uint64_t desc_addr;
    uint64_t avail_addr;
    uint64_t used_addr;
} virtqueue_t;

// VirtIO 设备结构
typedef struct virtio_device {
    uint32_t device_id;         // 设备ID
    uint32_t vendor_id;         // 厂商ID
    uint32_t version;           // 版本
    
    uint64_t device_features;   // 设备特性
    uint64_t driver_features;   // 驱动特性
    uint32_t features_sel;      // 特性选择器
    
    uint8_t status;             // 设备状态
    uint8_t config_generation;  // 配置生成号
    
    uint32_t queue_sel;         // 当前选择的队列
    virtqueue_t *queues;        // 队列数组
    uint32_t num_queues;        // 队列数量
    
    uint32_t interrupt_status;  // 中断状态
    
    // 设备特定配置
    void *config;               // 配置空间
    uint32_t config_len;        // 配置空间长度
    
    // 回调函数
    void (*queue_notify)(struct virtio_device *dev, uint32_t queue_idx);
    void (*reset)(struct virtio_device *dev);
    
    // 关联的VM
    struct _vm_t *vm;
    
    // MMIO 基地址
    uint64_t base_addr;
    uint32_t irq;               // 中断号
} virtio_device_t;

// 函数声明
void virtio_global_init(void);
virtio_device_t *virtio_create_device(uint32_t device_id, uint64_t base_addr, uint32_t irq);
void virtio_destroy_device(virtio_device_t *dev);

// MMIO 访问处理
bool virtio_mmio_read(virtio_device_t *dev, uint64_t offset, uint32_t *value, uint32_t size);
bool virtio_mmio_write(virtio_device_t *dev, uint64_t offset, uint32_t value, uint32_t size);

// VirtQueue 操作
int virtio_queue_init(virtqueue_t *vq, uint32_t num);
void virtio_queue_reset(virtqueue_t *vq);
bool virtio_queue_available(virtqueue_t *vq);
int virtio_queue_pop(virtqueue_t *vq, virtq_desc_t *desc_chain, uint32_t *desc_count);
void virtio_queue_push(virtqueue_t *vq, uint32_t desc_idx, uint32_t len);
void virtio_queue_notify(virtio_device_t *dev, uint32_t queue_idx);

// 中断处理
void virtio_inject_irq(virtio_device_t *dev, uint32_t int_type);

// 特性处理
bool virtio_has_feature(virtio_device_t *dev, uint32_t feature);
void virtio_set_features(virtio_device_t *dev, uint64_t features);

// 设备查找
virtio_device_t *virtio_find_device_by_addr(uint64_t addr);

// MMIO 陷入处理
bool handle_virtio_mmio_access(uint64_t addr, uint32_t *value, bool is_write, uint32_t size);

// 前向声明
struct virtio_console;
typedef struct virtio_console virtio_console_t;

// VirtIO Console 函数
virtio_console_t *virtio_console_create(uint64_t base_addr, uint32_t irq);
void virtio_console_destroy(virtio_console_t *console);

#endif // __VIRTIO_H__
