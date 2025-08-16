
#include "io.h"
#include "avatar_types.h"
#include "virtio_alloctor.h"


// External symbols from linker script
extern uint64_t __virtio_start;
extern uint64_t __virtio_end;

// Global allocator instance
static virtio_allocator_t g_allocator;
static virtio_allocator_t g_device_allocator; // Separate allocator for device queues
static bool g_allocator_initialized = false;

int virtio_allocator_init(void)
{
    if (g_allocator_initialized)
    {
        logger_warn("VirtIO allocator already initialized\n");
        return 0;
    }

    // Get memory region from linker script
    uint64_t virtio_start = (uint64_t)&__virtio_start;
    uint64_t virtio_end = (uint64_t)&__virtio_end;

    // First 0x500000 (5MB) for device queues
    g_device_allocator.start_addr = virtio_start;
    g_device_allocator.end_addr = virtio_start + 0x500000;
    g_device_allocator.current_addr = g_device_allocator.start_addr;
    g_device_allocator.total_size = 0x500000;
    g_device_allocator.used_size = 0;

    // Remaining space for general allocation
    g_allocator.start_addr = virtio_start + 0x500000;
    g_allocator.end_addr = virtio_end;
    g_allocator.current_addr = g_allocator.start_addr;
    g_allocator.total_size = g_allocator.end_addr - g_allocator.start_addr;
    g_allocator.used_size = 0;

    logger_info("VirtIO allocator initialized:\n");
    logger_info("  Device memory: 0x%lx - 0x%lx (%lu MB)\n",
              g_device_allocator.start_addr, g_device_allocator.end_addr,
              g_device_allocator.total_size / (1024 * 1024));
    logger_info("  General memory: 0x%lx - 0x%lx (%lu MB)\n",
              g_allocator.start_addr, g_allocator.end_addr,
              g_allocator.total_size / (1024 * 1024));

    g_allocator_initialized = true;
    return 0;
}

void *virtio_alloc(uint32_t size, uint32_t alignment)
{
    if (!g_allocator_initialized)
    {
        logger_error("VirtIO allocator not initialized\n");
        return NULL;
    }

    if (size == 0)
    {
        logger_error("Cannot allocate 0 bytes\n");
        return NULL;
    }

    // Default alignment is 8 bytes if not specified
    if (alignment == 0)
    {
        alignment = 8;
    }

    // Ensure alignment is power of 2
    if ((alignment & (alignment - 1)) != 0)
    {
        logger_error("Alignment must be power of 2: %u\n", alignment);
        return NULL;
    }

    // Align current address
    uint64_t aligned_addr = (g_allocator.current_addr + alignment - 1) & ~(alignment - 1);

    // Check if we have enough space
    if (aligned_addr + size > g_allocator.end_addr)
    {
        logger_error("Out of VirtIO memory: need %u bytes, have %lu bytes\n",
                   size, g_allocator.end_addr - aligned_addr);
        return NULL;
    }

    // Update allocator state
    g_allocator.current_addr = aligned_addr + size;
    g_allocator.used_size = g_allocator.current_addr - g_allocator.start_addr;

    logger_debug("VirtIO alloc: addr=0x%lx, size=%u, align=%u\n",
               aligned_addr, size, alignment);

    return (void *)aligned_addr;
}

void *virtio_alloc_aligned(uint32_t size, uint32_t alignment)
{
    return virtio_alloc(size, alignment);
}

uint64_t virtio_get_used_memory(void)
{
    if (!g_allocator_initialized)
    {
        return 0;
    }
    return g_allocator.used_size;
}

uint64_t virtio_get_free_memory(void)
{
    if (!g_allocator_initialized)
    {
        return 0;
    }
    return g_allocator.total_size - g_allocator.used_size;
}

void virtio_allocator_info(void)
{
    if (!g_allocator_initialized)
    {
        logger_error("VirtIO allocator not initialized\n");
        return;
    }

    logger_info("VirtIO Memory Allocator Status:\n");
    logger_info("  Total:     %lu bytes (%lu KB)\n",
              g_allocator.total_size, g_allocator.total_size / 1024);
    logger_info("  Used:      %lu bytes (%lu KB)\n",
              g_allocator.used_size, g_allocator.used_size / 1024);
    logger_info("  Free:      %lu bytes (%lu KB)\n",
              virtio_get_free_memory(), virtio_get_free_memory() / 1024);
    logger_info("  Current:   0x%lx\n", g_allocator.current_addr);
    logger_info("  Usage:     %lu%%\n",
              (g_allocator.used_size * 100) / g_allocator.total_size);
}

// Device-specific memory allocation functions
uint64_t virtio_get_device_base_addr(uint32_t device_index)
{
    if (!g_allocator_initialized)
    {
        logger_error("VirtIO allocator not initialized\n");
        return 0;
    }

    // Each device gets VIRTIO_DEVICE_MEMORY_SIZE bytes
    uint64_t device_base = g_device_allocator.start_addr + (device_index * VIRTIO_DEVICE_MEMORY_SIZE);

    // Check bounds
    if (device_base + VIRTIO_DEVICE_MEMORY_SIZE > g_device_allocator.end_addr)
    {
        logger_error("Device index %u exceeds available device memory\n", device_index);
        return 0;
    }

    return device_base;
}

uint64_t virtio_get_queue_desc_addr(uint32_t device_index, uint32_t queue_id)
{
    uint64_t device_base = virtio_get_device_base_addr(device_index);
    if (device_base == 0)
    {
        return 0;
    }

    // Each queue gets 0x4000 (16KB) within the device memory
    // Descriptor table is at the start of queue memory (0x1000 aligned)
    uint64_t queue_base = device_base + (queue_id * 0x4000);
    uint64_t desc_addr = (queue_base + 0xFFF) & ~0xFFF; // Ensure 0x1000 alignment

    logger_debug("Device %u Queue %u desc addr: 0x%lx\n", device_index, queue_id, desc_addr);
    return desc_addr;
}

uint64_t virtio_get_queue_avail_addr(uint32_t device_index, uint32_t queue_id)
{
    uint64_t desc_addr = virtio_get_queue_desc_addr(device_index, queue_id);
    if (desc_addr == 0)
    {
        return 0;
    }

    // Available ring is at desc + 0x100
    uint64_t avail_addr = desc_addr + VIRTIO_QUEUE_AVAIL_OFFSET;

    logger_debug("Device %u Queue %u avail addr: 0x%lx\n", device_index, queue_id, avail_addr);
    return avail_addr;
}

uint64_t virtio_get_queue_used_addr(uint32_t device_index, uint32_t queue_id)
{
    uint64_t avail_addr = virtio_get_queue_avail_addr(device_index, queue_id);
    if (avail_addr == 0)
    {
        return 0;
    }

    // Used ring is at avail + 0x1000
    uint64_t used_addr = avail_addr + VIRTIO_QUEUE_USED_OFFSET;

    logger_debug("Device %u Queue %u used addr: 0x%lx\n", device_index, queue_id, used_addr);
    return used_addr;
}
