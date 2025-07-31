
#ifndef MEM_H
#define MEM_H

#include "mem/bitmap.h"
#include "task/mutex.h"
#include "mem/page.h"
#include "os_cfg.h"

// 将物理地址映射到虚拟地址
#define phys_to_virt(pa) ((void *)((uint64_t)(pa) + KERNEL_VMA))

// 将虚拟地址转换为物理地址
#define virt_to_phys(va) ((uint64_t)(va) - KERNEL_VMA)

typedef struct _addr_alloc_t
{
    mutex_t mutex;      // for test
    bitmap_t bitmap;    // 辅助分配用的位图
    uint64_t page_size; // 页大小
    uint64_t start;     // 起始地址
    uint64_t size;      // 地址大小
} addr_alloc_t;

void alloctor_init();
void kmem_test();
uint64_t fs_malloc_pages(int32_t page_count);

uint64_t mutex_test_add();
uint64_t mutex_test_minus();
void mutex_test_print();

void *kalloc_pages(uint32_t);
void kfree_pages(void *addr, uint32_t pages);
pte_t *create_uvm(void);
uint64_t memory_alloc_page(pte_t *page_dir, uint64_t vaddr, uint64_t size, int32_t perm); // 为某个进程空间申请一块内存
uint64_t memory_get_paddr(pte_t *page_dir, uint64_t vaddr);
void destory_4level(pte_t *page_dir);
void destroy_uvm_4level(pte_t *page_dir);
int32_t memory_copy_uvm_4level(pte_t *dst_pgd, pte_t *src_pgd);
int32_t memory_create_map(pte_t *page_dir, uint64_t vaddr, uint64_t paddr, int32_t count, uint64_t perm);

#endif // MEM_H