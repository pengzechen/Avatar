// bitmap.h
#ifndef BITMAP_H
#define BITMAP_H

#include <aj_types.h>
#include <os_cfg.h>
#include <io.h>

typedef struct {
    uint8_t *bits;
    size_t size; // 以 bit 为单位的大小
} bitmap_t;

void bitmap_init(bitmap_t *bitmap, uint8_t *buffer, size_t size);
void bitmap_set(bitmap_t *bitmap, size_t index);
void bitmap_set_range(bitmap_t *bitmap, size_t start, size_t count);
void bitmap_clear(bitmap_t *bitmap, size_t index);
void bitmap_clear_range(bitmap_t *bitmap, size_t start, size_t count);
uint8_t bitmap_test(const bitmap_t *bitmap, size_t index);
uint64_t bitmap_find_first_free(const bitmap_t *bitmap);
uint64_t bitmap_find_contiguous_free(const bitmap_t *bitmap, size_t count);

extern uint8_t bitmap_buffer[OS_CFG_BITMAP_SIZE / 8] __attribute__((section(".bss.bitmap_buffer")));

#define assert(condition) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: %s, function %s, file %s, line %d\n", \
                    #condition, __func__, __FILE__, __LINE__); \
            while(1); \
        } \
    } while(0)

#endif // BITMAP_H