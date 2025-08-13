/**
 * @file pmm.c
 * @brief Physical Memory Manager Implementation - 物理内存管理器实现
 */

#include "mem/pmm.h"
#include "lib/avatar_assert.h"
#include "lib/avatar_string.h"
#include "io.h"
#include "os_cfg.h"

// 向上对齐到指定边界
#define ALIGN_UP(size, bound) (((size) + (bound) - 1) & ~((bound) - 1))

void pmm_init(pmm_t *pmm, uint64_t start_addr, uint64_t size,
              uint8_t *bitmap_buffer, size_t bitmap_size)
{
    avatar_assert(pmm != NULL);
    avatar_assert(bitmap_buffer != NULL);
    avatar_assert(size > 0);
    
    // 初始化基本信息
    pmm->start_addr = start_addr;
    pmm->total_size = size;
    pmm->page_size = PAGE_SIZE;
    pmm->total_pages = size / PAGE_SIZE;
    pmm->free_pages = pmm->total_pages;
    
    // 初始化位图
    bitmap_init(&pmm->bitmap, bitmap_buffer, bitmap_size);
    
    // 初始化互斥锁
    mutex_init(&pmm->mutex);
    
    logger("PMM initialized: start=0x%llx, size=0x%llx, pages=%llu\n",
           start_addr, size, pmm->total_pages);
}

uint64_t pmm_alloc_pages(pmm_t *pmm, uint32_t page_count)
{
    avatar_assert(pmm != NULL);
    avatar_assert(page_count > 0);
    
    uint64_t paddr = 0;
    
    // mutex_lock(&pmm->mutex);  // 暂时注释掉，保持与原代码一致
    
    // 查找连续的空闲页面
    int32_t page_index = bitmap_find_contiguous_free(&pmm->bitmap, page_count);
    if (page_index >= 0) {
        // 标记页面为已分配
        bitmap_set_range(&pmm->bitmap, page_index, page_count);
        
        // 计算物理地址
        paddr = pmm->start_addr + page_index * pmm->page_size;
        
        // 更新空闲页面计数
        pmm->free_pages -= page_count;
        
        // 调试输出，确认是否设置了位图
        if (bitmap_test(&pmm->bitmap, page_index)) {
            // logger("Page index %d marked as allocated.\n", page_index);
        }
    }
    
    // mutex_unlock(&pmm->mutex);  // 暂时注释掉，保持与原代码一致
    
    return paddr;
}

uint64_t pmm_alloc_pages_fs(pmm_t *pmm, uint32_t page_count)
{
    avatar_assert(pmm != NULL);
    avatar_assert(page_count > 0);
    
    uint64_t paddr = 0;
    
    // 使用first-fit策略查找连续的空闲页面
    int32_t page_index = bitmap_find_contiguous_free_fs(&pmm->bitmap, page_count);
    if (page_index >= 0) {
        // 标记页面为已分配
        bitmap_set_range(&pmm->bitmap, page_index, page_count);
        
        // 计算物理地址
        paddr = pmm->start_addr + page_index * pmm->page_size;
        
        // 更新空闲页面计数
        pmm->free_pages -= page_count;
        
        // 调试输出，确认是否设置了位图
        if (bitmap_test(&pmm->bitmap, page_index)) {
            logger("Page index %d marked as allocated, count: %d, addr 0x%llx\n", 
                   page_index, page_count, paddr);
        }
    }
    
    return paddr;
}

void pmm_free_pages(pmm_t *pmm, uint64_t paddr, uint32_t page_count)
{
    avatar_assert(pmm != NULL);
    avatar_assert(page_count > 0);
    
    // 检查地址范围
    if (paddr < pmm->start_addr || paddr >= pmm->start_addr + pmm->total_size) {
        logger("PMM: Invalid free address: 0x%llx\n", paddr);
        return;
    }
    
    // 计算页面索引
    uint64_t page_index = (paddr - pmm->start_addr) / pmm->page_size;
    
    // 检查范围
    if (page_index + page_count <= pmm->total_pages) {
        // 清除位图标记
        bitmap_clear_range(&pmm->bitmap, page_index, page_count);
        
        // 更新空闲页面计数
        pmm->free_pages += page_count;
    } else {
        logger("PMM: Invalid free request: paddr=0x%llx, count=%u\n", 
               paddr, page_count);
    }
}

void pmm_mark_allocated(pmm_t *pmm, uint64_t start_addr, uint64_t end_addr)
{
    avatar_assert(pmm != NULL);
    avatar_assert(start_addr <= end_addr);
    
    // 计算页面范围
    uint64_t start_page = (start_addr - pmm->start_addr) / pmm->page_size;
    uint64_t end_page = (end_addr - pmm->start_addr) / pmm->page_size;
    
    // 遍历所有页，并在bitmap中标记为已分配
    for (uint64_t i = start_page; i <= end_page; i++) {
        bitmap_set(&pmm->bitmap, i);
        pmm->free_pages--;
    }
    
    logger("Marked memory from 0x%llx to 0x%llx as allocated. index start: %llu, count: %llu\n\n", 
           start_addr, end_addr, start_page, end_page - start_page + 1);
}

uint64_t pmm_get_free_pages(pmm_t *pmm)
{
    avatar_assert(pmm != NULL);
    return pmm->free_pages;
}

uint64_t pmm_get_total_pages(pmm_t *pmm)
{
    avatar_assert(pmm != NULL);
    return pmm->total_pages;
}

void pmm_mark_kernel_allocated(pmm_t *pmm)
{
    extern char __kernal_start[];
    extern char __heap_flag[];

    uint64_t start = (uint64_t)__kernal_start;
    uint64_t end = (uint64_t)__heap_flag + HEAP_OFFSET;
    // 如果 heap_start 不是页对齐的，将其向上对齐
    end = ALIGN_UP(end, PAGE_SIZE);

    logger("PMM: Marking kernel memory 0x%llx-0x%llx as allocated\n", start, end);
    pmm_mark_allocated(pmm, start, end);
}
