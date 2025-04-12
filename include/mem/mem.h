
#ifndef MEM_H
#define MEM_H

#include "mem/bitmap.h"
#include "task/mutex.h"

typedef struct _addr_alloc_t {
    mutex_t mutex; // for test
    bitmap_t bitmap;            // 辅助分配用的位图
    uint64_t page_size;         // 页大小
    uint64_t start;             // 起始地址
    uint64_t size;              // 地址大小
} addr_alloc_t;

void alloctor_init() ;


uint64_t mutex_test_add() ;

uint64_t mutex_test_minus() ;

void mutex_test_print();

#endif // MEM_H