#ifndef _VIRTIO_ALLOCATOR_H
#define _VIRTIO_ALLOCATOR_H

#include "avatar_types.h"

// VirtIO memory allocator structure
typedef struct
{
    uint64_t start_addr;
    uint64_t end_addr;
    uint64_t current_addr;
    uint64_t total_size;
    uint64_t used_size;
} virtio_allocator_t;

// Device memory layout constants
#define VIRTIO_DEVICE_MEMORY_SIZE 0x80000 // 512KB per device
#define VIRTIO_QUEUE_DESC_OFFSET 0x0      // Descriptor table at start (0x1000 aligned)
#define VIRTIO_QUEUE_AVAIL_OFFSET 0x100   // Available ring at desc + 0x100
#define VIRTIO_QUEUE_USED_OFFSET 0xf00    // Used ring at avail + 0x1000

// Function declarations
int virtio_allocator_init(void);
void *virtio_alloc(uint32_t size, uint32_t alignment);
void *virtio_alloc_aligned(uint32_t size, uint32_t alignment);
void virtio_free(void *ptr);
uint64_t virtio_get_used_memory(void);
uint64_t virtio_get_free_memory(void);
void virtio_allocator_info(void);

// Device-specific memory allocation
uint64_t virtio_get_device_base_addr(uint32_t device_index);
uint64_t virtio_get_queue_desc_addr(uint32_t device_index, uint32_t queue_id);
uint64_t virtio_get_queue_avail_addr(uint32_t device_index, uint32_t queue_id);
uint64_t virtio_get_queue_used_addr(uint32_t device_index, uint32_t queue_id);

// Test function
void virtio_allocator_test(void);

#endif // _VIRTIO_ALLOCATOR_H