

#include <mem/bitmap.h>
#include <os_cfg.h>

//  0x20000  128Kb
static uint8_t  bitmap_buffer[OS_CFG_BITMAP_SIZE / 8] __attribute__((section(".data.bitmap_buffer")));
static bitmap_t  bitmap = {bitmap_buffer, OS_CFG_BITMAP_SIZE};

void bitmap_init(bitmap_t *bitmap, uint8_t *buffer, size_t size) {
    bitmap->bits = buffer;
    bitmap->size = size;
    for (size_t i = 0; i < (size + 7) / 8; i++) {
        buffer[i] = 0;
    }
}

void bitmap_set(bitmap_t *bitmap, size_t index) {
    if (index < bitmap->size) {
        bitmap->bits[index / 8] |= (1 << (index % 8));
    }
}

void bitmap_clear(bitmap_t *bitmap, size_t index) {
    if (index < bitmap->size) {
        bitmap->bits[index / 8] &= ~(1 << (index % 8));
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

void bitmap_create_from_memory(bitmap_t *bitmap, uint64_t phys_start, size_t phys_size) {
    if (!bitmap || !phys_start || !phys_size) {
        return; // 防止无效参数
    }

    uint8_t *buffer = (uint8_t *)phys_start; // 假设已经映射到虚拟地址
    size_t bit_size = phys_size / PAGE_SIZE; // 计算物理页数量

    bitmap_init(bitmap, buffer, bit_size);
}