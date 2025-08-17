
#include "io.h"
#include "avatar_types.h"
#include "virtio_alloctor.h"

// Free block structure for managing freed memory
typedef struct free_block {
    uint32_t size;              // Size of this free block
    struct free_block *next;    // Next free block in the list
} free_block_t;

// External symbols from linker script
extern uint64_t __virtio_start;
extern uint64_t __virtio_end;

// Global allocator instance
static virtio_allocator_t g_allocator;
static virtio_allocator_t g_device_allocator; // Separate allocator for device queues
static bool g_allocator_initialized = false;

// Free list head for managing freed blocks
static free_block_t *g_free_list_head = NULL;

// Forward declaration for helper function
static void virtio_coalesce_free_blocks(void);

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

    // Add space for size header (4 bytes) to track allocation size
    uint32_t total_size = size + sizeof(uint32_t);

    // First try to find a suitable free block
    free_block_t **current = &g_free_list_head;
    while (*current != NULL)
    {
        free_block_t *block = *current;
        uint64_t block_addr = (uint64_t)block;
        uint64_t aligned_addr = (block_addr + sizeof(uint32_t) + alignment - 1) & ~(alignment - 1);
        uint32_t needed_size = (aligned_addr - block_addr) + size;

        if (block->size >= needed_size)
        {
            // Remove this block from free list
            *current = block->next;

            // If block is larger than needed, split it
            if (block->size > needed_size + sizeof(free_block_t))
            {
                free_block_t *new_block = (free_block_t *)((uint8_t *)block + needed_size);
                new_block->size = block->size - needed_size;
                new_block->next = g_free_list_head;
                g_free_list_head = new_block;
            }

            // Store allocation size at the beginning
            *(uint32_t *)block = size;

            logger_debug("VirtIO alloc from free list: addr=0x%lx, size=%u, align=%u\n",
                       aligned_addr, size, alignment);

            return (void *)aligned_addr;
        }
        current = &((*current)->next);
    }

    // No suitable free block found, allocate from the end
    uint64_t aligned_addr = (g_allocator.current_addr + sizeof(uint32_t) + alignment - 1) & ~(alignment - 1);
    uint32_t needed_size = (aligned_addr - g_allocator.current_addr) + size;

    // Check if we have enough space
    if (g_allocator.current_addr + needed_size > g_allocator.end_addr)
    {
        logger_error("Out of VirtIO memory: need %u bytes, have %lu bytes\n",
                   needed_size, g_allocator.end_addr - g_allocator.current_addr);
        return NULL;
    }

    // Store allocation size at the beginning
    *(uint32_t *)g_allocator.current_addr = size;

    // Update allocator state
    g_allocator.current_addr += needed_size;
    g_allocator.used_size = g_allocator.current_addr - g_allocator.start_addr;

    logger_debug("VirtIO alloc from end: addr=0x%lx, size=%u, align=%u\n",
               aligned_addr, size, alignment);

    return (void *)aligned_addr;
}

void virtio_free(void *ptr)
{
    if (!ptr) return;

    if (!g_allocator_initialized)
    {
        logger_error("VirtIO allocator not initialized\n");
        return;
    }

    // Check if pointer is within our managed range
    uint64_t addr = (uint64_t)ptr;
    if (addr < g_allocator.start_addr || addr >= g_allocator.end_addr)
    {
        logger_error("Invalid free address: 0x%lx (not in range 0x%lx - 0x%lx)\n",
                   addr, g_allocator.start_addr, g_allocator.end_addr);
        return;
    }

    // Get the allocation size from the header (stored before the aligned address)
    // We need to find the start of the allocation block
    uint64_t block_start = addr - sizeof(uint32_t);

    // Search backwards to find the size header
    // This is a simple approach - in a more sophisticated allocator,
    // we would store metadata more systematically
    while (block_start >= g_allocator.start_addr)
    {
        uint32_t stored_size = *(uint32_t *)block_start;
        uint64_t expected_end = block_start + sizeof(uint32_t) + stored_size;

        // Check if this could be our allocation
        if (expected_end >= addr && expected_end <= addr + 64) // Allow some alignment slack
        {
            // Found the allocation header
            uint32_t total_size = sizeof(uint32_t) + stored_size;

            // Create a new free block
            free_block_t *new_block = (free_block_t *)block_start;
            new_block->size = total_size;

            // Insert into free list (simple insertion at head)
            new_block->next = g_free_list_head;
            g_free_list_head = new_block;

            // Try to coalesce with adjacent free blocks
            virtio_coalesce_free_blocks();

            logger_debug("VirtIO free: addr=0x%lx, size=%u, block_start=0x%lx\n",
                       addr, stored_size, block_start);
            return;
        }

        block_start -= 4; // Try previous 4-byte aligned position
    }

    logger_error("Could not find allocation header for address 0x%lx\n", addr);
}

// Helper function to coalesce adjacent free blocks
static void virtio_coalesce_free_blocks(void)
{
    if (!g_free_list_head) return;

    // Simple coalescing: check if any two blocks are adjacent
    free_block_t *current = g_free_list_head;
    while (current != NULL)
    {
        free_block_t **next_ptr = &current->next;
        while (*next_ptr != NULL)
        {
            free_block_t *next_block = *next_ptr;
            uint64_t current_end = (uint64_t)current + current->size;
            uint64_t next_start = (uint64_t)next_block;

            if (current_end == next_start)
            {
                // Coalesce: merge next_block into current
                current->size += next_block->size;
                *next_ptr = next_block->next;
                logger_debug("Coalesced blocks: 0x%lx + 0x%lx (size %u)\n",
                           (uint64_t)current, next_start, current->size);
            }
            else if (next_start + next_block->size == (uint64_t)current)
            {
                // Coalesce: merge current into next_block
                next_block->size += current->size;
                // Remove current from list and restart
                if (current == g_free_list_head)
                {
                    g_free_list_head = current->next;
                    current = g_free_list_head;
                    break;
                }
                else
                {
                    // Find previous node to update its next pointer
                    free_block_t *prev = g_free_list_head;
                    while (prev && prev->next != current)
                    {
                        prev = prev->next;
                    }
                    if (prev)
                    {
                        prev->next = current->next;
                        current = prev;
                        break;
                    }
                }
            }
            else
            {
                next_ptr = &((*next_ptr)->next);
            }
        }
        current = current->next;
    }
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

    // Calculate actual used memory by subtracting free blocks
    uint64_t total_used = g_allocator.current_addr - g_allocator.start_addr;
    uint64_t free_size = 0;

    free_block_t *current = g_free_list_head;
    while (current != NULL)
    {
        free_size += current->size;
        current = current->next;
    }

    return total_used - free_size;
}

uint64_t virtio_get_free_memory(void)
{
    if (!g_allocator_initialized)
    {
        return 0;
    }

    // Free memory = unallocated space + freed blocks
    uint64_t unallocated = g_allocator.end_addr - g_allocator.current_addr;
    uint64_t freed_blocks = 0;

    free_block_t *current = g_free_list_head;
    while (current != NULL)
    {
        freed_blocks += current->size;
        current = current->next;
    }

    return unallocated + freed_blocks;
}

void virtio_allocator_info(void)
{
    if (!g_allocator_initialized)
    {
        logger_error("VirtIO allocator not initialized\n");
        return;
    }

    uint64_t used_memory = virtio_get_used_memory();
    uint64_t free_memory = virtio_get_free_memory();
    uint64_t allocated_space = g_allocator.current_addr - g_allocator.start_addr;

    // Count free blocks
    uint32_t free_block_count = 0;
    uint64_t total_freed_size = 0;
    free_block_t *current = g_free_list_head;
    while (current != NULL)
    {
        free_block_count++;
        total_freed_size += current->size;
        current = current->next;
    }

    logger_info("VirtIO Memory Allocator Status:\n");
    logger_info("  Total:         %lu bytes (%lu KB)\n",
              g_allocator.total_size, g_allocator.total_size / 1024);
    logger_info("  Used:          %lu bytes (%lu KB)\n",
              used_memory, used_memory / 1024);
    logger_info("  Free:          %lu bytes (%lu KB)\n",
              free_memory, free_memory / 1024);
    logger_info("  Allocated:     %lu bytes (%lu KB)\n",
              allocated_space, allocated_space / 1024);
    logger_info("  Freed blocks:  %u blocks, %lu bytes\n",
              free_block_count, total_freed_size);
    logger_info("  Current ptr:   0x%lx\n", g_allocator.current_addr);
    logger_info("  Usage:         %lu%%\n",
              g_allocator.total_size > 0 ? (used_memory * 100) / g_allocator.total_size : 0);

    // Show free block details if any
    if (free_block_count > 0)
    {
        logger_info("  Free block list:\n");
        current = g_free_list_head;
        uint32_t i = 0;
        while (current != NULL && i < 10) // Show first 10 blocks
        {
            logger_info("    Block %u: addr=0x%lx, size=%u bytes\n",
                      i, (uint64_t)current, current->size);
            current = current->next;
            i++;
        }
        if (current != NULL)
        {
            logger_info("    ... and %u more blocks\n", free_block_count - 10);
        }
    }
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

// Test function for the allocator
void virtio_allocator_test(void)
{
    if (!g_allocator_initialized)
    {
        logger_error("VirtIO allocator not initialized for testing\n");
        return;
    }

    logger_info("=== VirtIO Allocator Test ===\n");

    // Show initial state
    logger_info("Initial state:\n");
    virtio_allocator_info();

    // Test 1: Basic allocation and free
    logger_info("\nTest 1: Basic allocation and free\n");
    void *ptr1 = virtio_alloc(64, 8);
    void *ptr2 = virtio_alloc(128, 16);
    void *ptr3 = virtio_alloc(256, 32);

    logger_info("Allocated ptr1=%p, ptr2=%p, ptr3=%p\n", ptr1, ptr2, ptr3);
    virtio_allocator_info();

    // Free middle allocation
    logger_info("\nFreeing ptr2 (middle allocation)\n");
    virtio_free(ptr2);
    virtio_allocator_info();

    // Allocate something that should fit in the freed space
    logger_info("\nAllocating 100 bytes (should reuse freed space)\n");
    void *ptr4 = virtio_alloc(100, 8);
    logger_info("Allocated ptr4=%p\n", ptr4);
    virtio_allocator_info();

    // Free all remaining
    logger_info("\nFreeing all remaining allocations\n");
    virtio_free(ptr1);
    virtio_free(ptr3);
    virtio_free(ptr4);
    virtio_allocator_info();

    logger_info("=== VirtIO Allocator Test Complete ===\n");
}
