
#ifndef MEM_H
#define MEM_H

#include "mem/bitmap.h"
#include "task/mutex.h"
#include "mem/page.h"

typedef struct _addr_alloc_t {
    mutex_t mutex; // for test
    bitmap_t bitmap;            // 辅助分配用的位图
    uint64_t page_size;         // 页大小
    uint64_t start;             // 起始地址
    uint64_t size;              // 地址大小
} addr_alloc_t;

void alloctor_init() ;
void kmem_test() ;


uint64_t mutex_test_add() ;
uint64_t mutex_test_minus() ;
void mutex_test_print();

void * kalloc_page() ;
void kfree_page(void *addr) ;
pte_t * create_uvm (void) ;
uint64_t memory_alloc_page(pte_t * page_dir, uint64_t vaddr, uint64_t size, int perm);  // 为某个进程空间申请一块内存
uint64_t memory_get_paddr(pte_t * page_dir, uint64_t vaddr);
void destroy_uvm_4level(pte_t *page_dir);
int memory_create_map(pte_t *page_dir, uint64_t vaddr, uint64_t paddr, int count, uint64_t perm);

#endif // MEM_H