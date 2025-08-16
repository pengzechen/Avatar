#ifndef __VIRTIO_BLOCK_FRONTEND_H__
#define __VIRTIO_BLOCK_FRONTEND_H__

#include "avatar_types.h"

// VirtIO Block 设备 ID
#define VIRTIO_ID_BLOCK     2

// VirtIO MMIO 寄存器偏移
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

// VirtIO 1.0 传统模式寄存器（用于兼容性）
#define VIRTIO_MMIO_QUEUE_ALIGN         0x03c
#define VIRTIO_MMIO_QUEUE_PFN           0x040

// VirtIO 状态位
#define VIRTIO_STATUS_ACKNOWLEDGE       1
#define VIRTIO_STATUS_DRIVER            2
#define VIRTIO_STATUS_DRIVER_OK         4
#define VIRTIO_STATUS_FEATURES_OK       8
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET 64
#define VIRTIO_STATUS_FAILED            128

// VirtIO Magic Number
#define VIRTIO_MMIO_MAGIC               0x74726976  // "virt"

// VirtQueue 描述符标志
#define VIRTQ_DESC_F_NEXT       1
#define VIRTQ_DESC_F_WRITE      2
#define VIRTQ_DESC_F_INDIRECT   4

// VirtQueue 可用环标志
#define VIRTQ_AVAIL_F_NO_INTERRUPT  1

// VirtIO Block 请求类型
#define VIRTIO_BLK_T_IN         0   // 读
#define VIRTIO_BLK_T_OUT        1   // 写
#define VIRTIO_BLK_T_FLUSH      4   // 刷新

// VirtIO Block 状态
#define VIRTIO_BLK_S_OK         0
#define VIRTIO_BLK_S_IOERR      1
#define VIRTIO_BLK_S_UNSUPP     2

// VirtIO Block 特性位
#define VIRTIO_BLK_F_SIZE_MAX   1
#define VIRTIO_BLK_F_SEG_MAX    2
#define VIRTIO_BLK_F_GEOMETRY   4
#define VIRTIO_BLK_F_RO         5
#define VIRTIO_BLK_F_BLK_SIZE   6
#define VIRTIO_BLK_F_FLUSH      9
#define VIRTIO_BLK_F_TOPOLOGY   10

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
} virtq_used_t;

// VirtQueue 结构
typedef struct virtio_queue {
    uint32_t queue_id;          // 队列ID
    uint32_t queue_size;        // 队列大小
    uint16_t last_used_idx;     // 上次处理的已用索引
    uint16_t free_head;         // 空闲描述符链头
    uint32_t num_free;          // 空闲描述符数量
    
    // 队列内存布局
    uint64_t desc_addr;         // 描述符表地址
    uint64_t avail_addr;        // 可用环地址
    uint64_t used_addr;         // 已用环地址
    
    virtq_desc_t *desc;         // 描述符表
    virtq_avail_t *avail;       // 可用环
    virtq_used_t *used;         // 已用环
} virtio_queue_t;

// VirtIO 设备结构
typedef struct virtio_device {
    uint64_t base_addr;         // MMIO 基地址
    uint32_t device_index;      // 设备索引
    uint32_t device_id;         // 设备ID
    uint32_t vendor_id;         // 厂商ID
    uint32_t version;           // 版本
    uint64_t device_features;   // 设备特性
    uint64_t driver_features;   // 驱动特性
    uint32_t num_queues;        // 队列数量
    virtio_queue_t queues[16];  // 队列数组（最多16个）
} virtio_device_t;

// VirtIO Block 请求头
typedef struct virtio_blk_req {
    uint32_t type;              // 请求类型
    uint32_t reserved;
    uint64_t sector;            // 起始扇区
} virtio_blk_req_t;

// VirtIO Block 配置
typedef struct virtio_blk_config {
    uint64_t capacity;          // 设备容量（扇区数）
    uint32_t size_max;          // 最大段大小
    uint32_t seg_max;           // 最大段数
    uint32_t blk_size;          // 块大小
} virtio_blk_config_t;

// VirtIO Block 设备
typedef struct virtio_blk_device {
    virtio_device_t *dev;       // VirtIO 设备
    virtio_blk_config_t config; // 配置
    uint32_t block_size;        // 块大小
    uint64_t capacity;          // 容量
} virtio_blk_device_t;

// 静态内存池配置
#define VIRTIO_MAX_DEVICES      4
#define VIRTIO_QUEUE_SIZE       16
#define VIRTIO_BUFFER_POOL_SIZE 64
#define VIRTIO_BUFFER_SIZE      4096

// 函数声明
int virtio_blk_frontend_init(void);
int virtio_blk_init(virtio_blk_device_t *blk_dev, uint64_t base_addr, uint32_t device_index);
int virtio_blk_read_sector(virtio_blk_device_t *blk_dev, uint64_t sector, void *buffer, uint32_t count);
int virtio_blk_write_sector(virtio_blk_device_t *blk_dev, uint64_t sector, const void *buffer, uint32_t count);
void virtio_blk_get_config(virtio_blk_device_t *blk_dev);

// 底层 VirtIO 操作（使用项目中的 mmio.h 和 mem/barrier.h）
uint32_t virtio_read32(virtio_device_t *dev, uint64_t offset);
void virtio_write32(virtio_device_t *dev, uint64_t offset, uint32_t value);
uint64_t virtio_read64(virtio_device_t *dev, uint64_t offset);
void virtio_set_status(virtio_device_t *dev, uint32_t status);
uint32_t virtio_get_status(virtio_device_t *dev);

// 队列操作
int virtio_queue_setup(virtio_device_t *dev, uint32_t queue_id, uint32_t queue_size);
int virtio_queue_add_buf(virtio_device_t *dev, uint32_t queue_id, 
                        uint64_t *buffers, uint32_t *lengths, 
                        uint32_t out_num, uint32_t in_num);
int virtio_queue_kick(virtio_device_t *dev, uint32_t queue_id);
int virtio_queue_get_buf(virtio_device_t *dev, uint32_t queue_id, uint32_t *len);

// 内存管理
void *virtio_alloc_static(uint32_t size, uint32_t align);
void virtio_free_static(void *ptr);
uint64_t virtio_get_queue_desc_addr(uint32_t device_index, uint32_t queue_id);
uint64_t virtio_get_queue_avail_addr(uint32_t device_index, uint32_t queue_id);
uint64_t virtio_get_queue_used_addr(uint32_t device_index, uint32_t queue_id);

// 设备扫描
uint64_t scan_for_virtio_block_device(uint32_t device_id);

// 调试函数
void virtio_blk_print_info(virtio_blk_device_t *blk_dev);

// 设备检测函数
void virtio_detect_devices(void);

// Avatar 集成接口函数
int avatar_virtio_block_init(void);
virtio_blk_device_t *avatar_get_virtio_block_device(void);
int avatar_virtio_block_read(uint64_t sector, void *buffer, uint32_t sector_count);
int avatar_virtio_block_write(uint64_t sector, const void *buffer, uint32_t sector_count);
int avatar_virtio_block_get_info(uint64_t *capacity, uint32_t *block_size);
int avatar_virtio_block_test(void);
void avatar_virtio_block_print_status(void);

// VMM 后端接口函数
int vmm_backend_read_from_host_storage(uint64_t sector, void *buffer, uint32_t count);
int vmm_backend_write_to_host_storage(uint64_t sector, const void *buffer, uint32_t count);
int vmm_backend_get_storage_info(uint64_t *total_sectors, uint32_t *sector_size);

#endif /* __VIRTIO_BLOCK_FRONTEND_H__ */
