

#include "mem/bitmap.h"
#include "os_cfg.h"
#include "lib/avatar_assert.h"

//  0x20000  128Kb
uint8_t bitmap_buffer[OS_CFG_BITMAP_SIZE / 8] __attribute__((section(".bss.bitmap_buffer")));

void
bitmap_init(bitmap_t *bitmap, uint8_t *buffer, size_t size)
{
    bitmap->bits = buffer;
    bitmap->size = size;
    for (size_t i = 0; i < (size + 7) / 8; i++) {
        buffer[i] = 0;
    }
}

// 设置指定索引位置的位为 1，前提是该位为 0（未分配）
void
bitmap_set(bitmap_t *bitmap, size_t index)
{
    avatar_assert(index < bitmap->size);  // 确保索引不越界
    // 确保该位是未分配的（即当前值为 0）
    avatar_assert((bitmap->bits[index / 8] & (1 << (index % 8))) == 0);

    // 设置该位为 1
    bitmap->bits[index / 8] |= (1 << (index % 8));
}

// 设置指定范围的位为 1，前提是这些位为 0（未分配）
void
bitmap_set_range(bitmap_t *bitmap, size_t start, size_t count)
{
    if (start + count > bitmap->size) {
        return;  // 超出范围，直接返回
    }

    for (size_t i = 0; i < count; i++) {
        // 对每个位置执行设置操作
        bitmap_set(bitmap, start + i);
    }
}

// 清除指定索引位置的位为 0，前提是该位为 1（已分配）
void
bitmap_clear(bitmap_t *bitmap, size_t index)
{
    avatar_assert(index < bitmap->size);  // 确保索引不越界
    // 确保该位是已分配的（即当前值为 1）
    // assert((bitmap->bits[index / 8] & (1 << (index % 8))) != 0);
    if ((bitmap->bits[index / 8] & (1 << (index % 8))) == 0) {
        // logger("warnning: free a already freed page!, index: %d\n", index);
    }

    // 清除该位为 0
    bitmap->bits[index / 8] &= ~(1 << (index % 8));
}

// 清除指定范围的位为 0，前提是这些位为 1（已分配）
void
bitmap_clear_range(bitmap_t *bitmap, size_t start, size_t count)
{
    if (start + count > bitmap->size) {
        return;  // 超出范围，直接返回
    }

    for (size_t i = 0; i < count; i++) {
        // 对每个位置执行清除操作
        bitmap_clear(bitmap, start + i);
    }
}

uint8_t
bitmap_test(const bitmap_t *bitmap, size_t index)
{
    if (index < bitmap->size) {
        return (bitmap->bits[index / 8] & (1 << (index % 8))) != 0;
    }
    return 0;
}

uint64_t
bitmap_find_first_free(const bitmap_t *bitmap)
{
    for (size_t i = 0; i < bitmap->size; i++) {
        if (!bitmap_test(bitmap, i)) {
            return i;
        }
    }
    return -1;
}

uint64_t
bitmap_find_contiguous_free(const bitmap_t *bitmap, size_t count)
{
    for (size_t i = 0; i <= bitmap->size - count; i++) {
        size_t j;
        for (j = 0; j < count; j++) {
            if (bitmap_test(bitmap, i + j)) {
                break;
            }
        }
        if (j == count) {
            return i;
        }
    }
    return -1;
}

// 为文件系统分配内存
uint64_t
bitmap_find_contiguous_free_fs(const bitmap_t *bitmap, size_t count)
{
    // 计算偏移量，并将其转换为位图中的索引
    size_t base_offset = RAM_FS_MEM_START - KERNEL_RAM_START;  // 偏移量：0x10000000 (256MB)
    size_t start_index = base_offset / 0x1000;  // 4KB = 0x1000，所以除以 4KB 来转换为位图索引

    for (size_t i = start_index; i <= bitmap->size - count; i++) {
        size_t j;
        for (j = 0; j < count; j++) {
            if (bitmap_test(bitmap, i + j)) {
                break;  // 如果当前位为已分配，跳出内层循环
            }
        }
        if (j == count) {
            return i;  // 找到一个连续的空闲区域
        }
    }
    return -1;  // 如果没有找到足够的空闲区域，返回 -1
}