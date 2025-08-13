
#ifndef MEM_H
#define MEM_H

#include "mem/bitmap.h"
#include "task/mutex.h"
#include "mem/page.h"
#include "mem/pmm.h"
#include "os_cfg.h"

// 将物理地址映射到虚拟地址
#define phys_to_virt(pa) ((void *)((uint64_t)(pa) + KERNEL_VMA))

// 将虚拟地址转换为物理地址
#define virt_to_phys(va) ((uint64_t)(va) - KERNEL_VMA)

#define GET_PGD_INDEX(addr) ((addr >> 39) & 0x1ff)
#define GET_PUD_INDEX(addr) ((addr >> 30) & 0x1ff)
#define GET_PMD_INDEX(addr) ((addr >> 21) & 0x1ff)
#define GET_PTE_INDEX(addr) ((addr >> 12) & 0x1ff)

// 向上对齐到 bound 边界
#define UP2(size, bound) (((size) + (bound) - 1) & ~((bound) - 1))

// 向下对齐到 bound 边界
#define DOWN2(size, bound) ((size) & ~((bound) - 1))

// addr_alloc_t 结构已移除，所有内存管理现在由 PMM 统一处理

void alloctor_init();
void kmem_test();
// fs_malloc_pages 已删除，直接使用 pmm_alloc_pages_fs(&g_pmm, page_count)

// 全局物理内存管理器访问接口
extern pmm_t g_pmm;

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