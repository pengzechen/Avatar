

#include <mem/bitmap.h>
#include <os_cfg.h>

//  0x20000  128Kb
uint8_t  bitmap_buffer[OS_CFG_BITMAP_SIZE / 8] __attribute__((section(".data.bitmap_buffer")));


void bitmap_init(bitmap_t *bitmap, uint8_t *buffer, size_t size) {
    bitmap->bits = buffer;
    bitmap->size = size;
    for (size_t i = 0; i < (size + 7) / 8; i++) {
        buffer[i] = 0;
    }
}

// 设置指定索引位置的位为 1，前提是该位为 0（未分配）
void bitmap_set(bitmap_t *bitmap, size_t index) {
    assert(index < bitmap->size); // 确保索引不越界
    // 确保该位是未分配的（即当前值为 0）
    assert((bitmap->bits[index / 8] & (1 << (index % 8))) == 0);

    // 设置该位为 1
    bitmap->bits[index / 8] |= (1 << (index % 8));
}

// 设置指定范围的位为 1，前提是这些位为 0（未分配）
void bitmap_set_range(bitmap_t *bitmap, size_t start, size_t count) {
    if (start + count > bitmap->size) {
        return; // 超出范围，直接返回
    }

    for (size_t i = 0; i < count; i++) {
        // 对每个位置执行设置操作
        bitmap_set(bitmap, start + i);
    }
}

// 清除指定索引位置的位为 0，前提是该位为 1（已分配）
void bitmap_clear(bitmap_t *bitmap, size_t index) {
    assert(index < bitmap->size); // 确保索引不越界
    // 确保该位是已分配的（即当前值为 1）
    assert((bitmap->bits[index / 8] & (1 << (index % 8))) != 0);

    // 清除该位为 0
    bitmap->bits[index / 8] &= ~(1 << (index % 8));
}

// 清除指定范围的位为 0，前提是这些位为 1（已分配）
void bitmap_clear_range(bitmap_t *bitmap, size_t start, size_t count) {
    if (start + count > bitmap->size) {
        return; // 超出范围，直接返回
    }

    for (size_t i = 0; i < count; i++) {
        // 对每个位置执行清除操作
        bitmap_clear(bitmap, start + i);
    }
}

uint8_t bitmap_test(const bitmap_t *bitmap, size_t index) {
    if (index < bitmap->size) {
        return (bitmap->bits[index / 8] & (1 << (index % 8))) != 0;
    }
    return 0;
}

uint64_t bitmap_find_first_free(const bitmap_t *bitmap) {
    for (size_t i = 0; i < bitmap->size; i++) {
        if (!bitmap_test(bitmap, i)) {
            return i;
        }
    }
    return -1;
}

uint64_t bitmap_find_contiguous_free(const bitmap_t *bitmap, size_t count) {
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
