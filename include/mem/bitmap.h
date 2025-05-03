// bitmap.h
#ifndef BITMAP_H
#define BITMAP_H

#include <aj_types.h>

typedef struct {
    uint8_t *bits;
    size_t size; // 以 bit 为单位的大小
} bitmap_t;

void bitmap_init(bitmap_t *bitmap, uint8_t *buffer, size_t size);
void bitmap_set(bitmap_t *bitmap, size_t index);
void bitmap_clear(bitmap_t *bitmap, size_t index);
uint8_t bitmap_test(const bitmap_t *bitmap, size_t index);
uint64_t bitmap_find_first_free(const bitmap_t *bitmap);
uint64_t bitmap_find_contiguous_free(const bitmap_t *bitmap, size_t count);
void bitmap_create_from_memory(bitmap_t *bitmap, uint64_t phys_start, size_t phys_size) ;

#endif // BITMAP_H