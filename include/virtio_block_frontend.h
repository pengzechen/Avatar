#ifndef __VIRTIO_BLOCK_FRONTEND_H__
#define __VIRTIO_BLOCK_FRONTEND_H__

#include "avatar_types.h"
#include "mmio.h"

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
#define VIRTIO_MMIO_GUEST_PAGE_SIZE     0x028 // Legacy only
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
// VirtIO queue descriptor
typedef struct
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) virtq_desc_t;

// VirtIO available ring
typedef struct
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed)) virtq_avail_t;

// VirtIO used ring element
typedef struct
{
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) virtq_used_elem_t;

typedef struct
{
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[];
} __attribute__((packed)) virtq_used_t;


typedef struct
{
    uint32_t queue_id;
    uint32_t queue_size;
    uint64_t desc_addr;
    uint64_t avail_addr;
    uint64_t used_addr;
    virtq_desc_t *desc;
    virtq_avail_t *avail;
    virtq_used_t *used;
    uint16_t last_used_idx;
    uint16_t free_head;
    uint16_t num_free;
} virtio_queue_t;

typedef struct
{
    uint64_t base_addr;
    uint32_t version;
    uint32_t device_id;
    uint32_t vendor_id;
    uint64_t device_features;
    uint64_t driver_features;
    uint8_t status;
    uint32_t device_index;     // Device index for memory allocation
    virtio_queue_t queues[16]; // Support up to 16 queues
    uint32_t num_queues;
} virtio_device_t;



// VirtIO Block request header
typedef struct
{
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed)) virtio_blk_req_t;

// VirtIO Block configuration structure
typedef struct
{
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    struct
    {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;
    struct
    {
        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;
    uint8_t writeback;
    uint8_t unused0[3];
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
} __attribute__((packed)) virtio_blk_config_t;

// VirtIO Block device structure
typedef struct
{
    virtio_device_t *dev;
    virtio_blk_config_t config;
    uint32_t block_size;
    uint64_t capacity;
} virtio_blk_device_t;



// VirtIO 扫描配置
#define VIRTIO_SCAN_BASE_ADDR   0x0a000000  // Start scanning from 0x0a00_0000
#define VIRTIO_SCAN_STEP        0x200       // Step size 0x200
#define VIRTIO_SCAN_COUNT       32          // Scan 32 positions



// 函数声明
int virtio_blk_frontend_init(void);
int virtio_blk_init(virtio_blk_device_t *blk_dev, uint64_t base_addr, uint32_t device_index);
int virtio_blk_read_sector(virtio_blk_device_t *blk_dev, uint64_t sector, void *buffer, uint32_t count);
int virtio_blk_write_sector(virtio_blk_device_t *blk_dev, uint64_t sector, const void *buffer, uint32_t count);
void virtio_blk_get_config(virtio_blk_device_t *blk_dev);

// 队列操作
int virtio_queue_setup(virtio_device_t *dev, uint32_t queue_id, uint32_t queue_size);
int virtio_queue_add_buf(virtio_device_t *dev, uint32_t queue_id, 
                        uint64_t *buffers, uint32_t *lengths, 
                        uint32_t out_num, uint32_t in_num);
int virtio_queue_kick(virtio_device_t *dev, uint32_t queue_id);
int virtio_queue_get_buf(virtio_device_t *dev, uint32_t queue_id, uint32_t *len);




// 设备扫描
uint64_t scan_for_virtio_block_device(uint32_t device_id);

// 调试函数
void virtio_blk_print_info(virtio_blk_device_t *blk_dev);
int virtio_block_test(void);

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




// MMIO 读写操作 - 使用项目中已有的安全 MMIO 函数
static inline uint32_t virtio_read32(virtio_device_t *dev, uint64_t offset)
{
    return mmio_read32((volatile void*)(dev->base_addr + offset));
}

static inline void virtio_write32(virtio_device_t *dev, uint64_t offset, uint32_t value)
{
    mmio_write32(value, (volatile void*)(dev->base_addr + offset));
}

static inline uint64_t virtio_read64(virtio_device_t *dev, uint64_t offset)
{
    return mmio_read64((volatile void*)(dev->base_addr + offset));
}

static inline void virtio_set_status(virtio_device_t *dev, uint32_t status)
{
    uint32_t current = virtio_read32(dev, VIRTIO_MMIO_STATUS);
    virtio_write32(dev, VIRTIO_MMIO_STATUS, current | status);
}

static inline uint32_t virtio_get_status(virtio_device_t *dev)
{
    return virtio_read32(dev, VIRTIO_MMIO_STATUS);
}


#endif /* __VIRTIO_BLOCK_FRONTEND_H__ */
