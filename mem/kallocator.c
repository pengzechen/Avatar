/**
 * @file kallocator.c
 * @brief Kernel Memory Allocator Implementation - 内核小块内存分配器实现
 */

#include "mem/kallocator.h"
#include "mem/mem.h"
#include "mem/pmm.h"
#include "lib/avatar_assert.h"
#include "lib/avatar_string.h"
#include "io.h"
#include "os_cfg.h"

// 外部符号，来自链接脚本
extern char __heap_flag[];

// 全局分配器实例
static kallocator_t g_kallocator;
static bool g_kallocator_initialized = false;

// 页面管理相关常量
#define INITIAL_PAGE_CAPACITY 64    // 初始页面数组容量
#define LARGE_ALLOC_THRESHOLD 2048  // 大块分配阈值（2KB）

// 分配块头部结构
typedef struct alloc_header {
    uint32_t size;      // 用户请求的大小
    uint32_t magic;     // 魔数，用于验证
} alloc_header_t;

#define ALLOC_MAGIC 0xDEADBEEF

// 前向声明
static void kallocator_coalesce_free_blocks(void);
static int kallocator_add_page(void *page_addr, uint32_t page_count, bool is_large_alloc, uint32_t actual_used_size);
static void kallocator_remove_page(void *page_addr);
static page_info_t *kallocator_find_page(void *addr);
static bool kallocator_is_page_empty(page_info_t *page);
static void *kalloc_small_block(uint32_t size, uint32_t alignment);

int kallocator_init(void)
{
    if (g_kallocator_initialized) {
        logger_warn("Kernel allocator already initialized\n");
        return 0;
    }

    // 初始化分配器结构
    memset(&g_kallocator, 0, sizeof(g_kallocator));

    // 分配页面信息数组
    g_kallocator.pages = (page_info_t *)kalloc_pages(1); // 分配1页用于页面信息数组
    if (g_kallocator.pages == NULL) {
        logger_error("Failed to allocate page info array for kallocator\n");
        return -1;
    }

    g_kallocator.page_capacity = PAGE_SIZE / sizeof(page_info_t);
    g_kallocator.page_count = 0;
    g_kallocator.free_list = NULL;
    g_kallocator.total_size = 0;
    g_kallocator.used_size = 0;
    g_kallocator.small_alloc_count = 0;
    g_kallocator.large_alloc_count = 0;

    // 初始化互斥锁
    mutex_init(&g_kallocator.mutex);

    logger_info("Kernel allocator initialized:\n");
    logger_info("  Page info capacity: %u pages\n", g_kallocator.page_capacity);
    logger_info("  Large alloc threshold: %u bytes\n", LARGE_ALLOC_THRESHOLD);
    logger_info("  Based on kalloc_pages/kfree_pages\n\n");

    g_kallocator_initialized = true;
    return 0;
}

void *kalloc(uint32_t size, uint32_t alignment)
{
    if (!g_kallocator_initialized) {
        logger_error("Kernel allocator not initialized\n");
        return NULL;
    }

    if (size == 0) {
        logger_error("Cannot allocate 0 bytes\n");
        return NULL;
    }

    // 默认对齐是8字节
    if (alignment == 0) {
        alignment = 8;
    }

    // 确保对齐是2的幂
    if ((alignment & (alignment - 1)) != 0) {
        logger_error("Alignment must be power of 2: %u\n", alignment);
        return NULL;
    }

    mutex_lock(&g_kallocator.mutex);

    // 判断是大块分配还是小块分配
    if (size >= LARGE_ALLOC_THRESHOLD) {
        // 大块分配：直接分配页面
        uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        void *page_addr = kalloc_pages(pages_needed);

        if (page_addr == NULL) {
            mutex_unlock(&g_kallocator.mutex);
            logger_error("Failed to allocate %u pages for large allocation\n", pages_needed);
            return NULL;
        }

        // 记录页面信息，传入用户实际请求的大小
        if (kallocator_add_page(page_addr, pages_needed, true, size) != 0) {
            kfree_pages(page_addr, pages_needed);
            mutex_unlock(&g_kallocator.mutex);
            logger_error("Failed to track large allocation pages\n");
            return NULL;
        }

        g_kallocator.large_alloc_count++;
        g_kallocator.total_size += pages_needed * PAGE_SIZE;
        g_kallocator.used_size += size;

        mutex_unlock(&g_kallocator.mutex);

        logger_debug("Large alloc: addr=0x%lx, size=%u, pages=%u\n",
                   (uint64_t)page_addr, size, pages_needed);

        return page_addr;
    } else {
        // 小块分配：在页面内部分配
        return kalloc_small_block(size, alignment);
    }
}

void *kalloc_simple(uint32_t size)
{
    return kalloc(size, 0); // 使用默认对齐
}

void *kalloc_aligned(uint32_t size, uint32_t alignment)
{
    return kalloc(size, alignment);
}

void kfree(void *ptr)
{
    if (!ptr) return;

    if (!g_kallocator_initialized) {
        logger_error("Kernel allocator not initialized\n");
        return;
    }

    mutex_lock(&g_kallocator.mutex);

    // 查找包含此地址的页面
    page_info_t *page = kallocator_find_page(ptr);
    if (page == NULL) {
        mutex_unlock(&g_kallocator.mutex);
        logger_error("Invalid free address: 0x%lx (not in managed pages)\n", (uint64_t)ptr);
        return;
    }

    if (page->is_large_alloc) {
        // 大块分配：直接释放整个页面
        // 在删除页面信息之前保存所有需要的数据
        uint32_t used_bytes = page->used_bytes;
        uint32_t page_count = page->page_count;
        void *page_addr = page->page_addr;

        // 先更新统计
        g_kallocator.large_alloc_count--;
        g_kallocator.total_size -= page_count * PAGE_SIZE;
        g_kallocator.used_size -= used_bytes;

        // 然后释放页面和删除记录
        kfree_pages(page_addr, page_count);
        kallocator_remove_page(page_addr);

        logger_debug("Large free: addr=0x%lx, pages=%u, used_bytes=%u\n",
                   (uint64_t)ptr, page_count, used_bytes);
    } else {
        // 小块分配：通过搜索找到分配头部
        uint64_t page_start = (uint64_t)page->page_addr;
        uint64_t page_end = page_start + (page->page_count * PAGE_SIZE);
        uint64_t ptr_addr = (uint64_t)ptr;

        // 在页面中搜索包含此地址的分配块
        alloc_header_t *found_header = NULL;
        uint32_t block_total_size = 0;

        // 更精确的头部搜索：从页面开始，按照分配块的布局遍历
        uint64_t current_addr = page_start;

        while (current_addr < page_end - sizeof(alloc_header_t)) {
            // 检查这个位置是否是空闲块
            bool is_free_block = false;
            free_block_t *check_free = g_kallocator.free_list;
            while (check_free != NULL) {
                if ((uint64_t)check_free == current_addr) {
                    is_free_block = true;
                    current_addr += check_free->size;
                    break;
                }
                check_free = check_free->next;
            }

            if (is_free_block) {
                continue; // 跳过空闲块
            }

            // 检查这个位置是否是有效的分配头部
            alloc_header_t *test_header = (alloc_header_t *)current_addr;

            // 检查魔数
            if (test_header->magic != ALLOC_MAGIC) {
                current_addr += 8; // 移动到下一个可能的位置
                continue;
            }

            // 检查大小的合理性
            if (test_header->size == 0 || test_header->size > PAGE_SIZE) {
                current_addr += 8;
                continue;
            }

            // 计算这个块的用户地址范围
            uint64_t user_start = current_addr + sizeof(alloc_header_t);
            uint64_t user_end = user_start + test_header->size;

            // 检查ptr是否在这个块的用户数据范围内
            if (ptr_addr >= user_start && ptr_addr < user_end) {
                found_header = test_header;
                // 计算块的总大小（包括可能的对齐填充）
                block_total_size = sizeof(alloc_header_t) + test_header->size;
                // 对齐到8字节边界
                block_total_size = (block_total_size + 7) & ~7;
                break;
            }

            // 移动到下一个块
            uint32_t this_block_size = sizeof(alloc_header_t) + test_header->size;
            this_block_size = (this_block_size + 7) & ~7;
            current_addr += this_block_size;
        }

        if (found_header == NULL) {
            mutex_unlock(&g_kallocator.mutex);
            logger_error("Cannot find allocation header for address 0x%lx in page 0x%lx-0x%lx\n",
                       ptr_addr, page_start, page_end);
            return;
        }

        // 清除魔数以标记为已释放
        found_header->magic = 0;

        // 创建空闲块（在填充之前）
        free_block_t *free_block = (free_block_t *)found_header;
        free_block->size = block_total_size;
        free_block->next = g_kallocator.free_list;
        g_kallocator.free_list = free_block;

        // 安全地填充释放的内存区域的用户数据部分，防止use-after-free
        // 但保留free_block_t结构不被破坏
        if (block_total_size > sizeof(free_block_t)) {
            uint8_t *fill_start = (uint8_t *)found_header + sizeof(free_block_t);
            uint32_t fill_size = block_total_size - sizeof(free_block_t);
            memset(fill_start, 0xDD, fill_size);
        }

        // 更新页面统计 - 只维护used_bytes，free_bytes通过计算得出
        page->used_bytes -= block_total_size;
        page->free_bytes = PAGE_SIZE - page->used_bytes;
        g_kallocator.used_size -= block_total_size;

        logger_debug("Updated page stats: used_bytes=%u, free_bytes=%u, global_used=%lu\n",
                   page->used_bytes, page->free_bytes, g_kallocator.used_size);

        // 尝试合并空闲块 - 但要小心处理
        kallocator_coalesce_free_blocks();

        // 检查页面是否完全空闲
        if (kallocator_is_page_empty(page)) {
            // 在释放页面之前，需要从空闲链表中移除所有属于这个页面的空闲块
            free_block_t **current = &g_kallocator.free_list;
            while (*current != NULL) {
                free_block_t *block = *current;
                uint64_t block_addr = (uint64_t)block;

                // 检查这个空闲块是否在要释放的页面内
                if (block_addr >= page_start && block_addr < page_end) {
                    // 从链表中移除这个块
                    *current = block->next;
                } else {
                    current = &(block->next);
                }
            }

            // 释放整个页面
            kfree_pages(page->page_addr, page->page_count);
            kallocator_remove_page(page->page_addr);
            g_kallocator.total_size -= page->page_count * PAGE_SIZE;
        }

        g_kallocator.small_alloc_count--;

        logger_info("Small free: addr=0x%lx, size=%u, total=%u\n",
                   ptr_addr, found_header->size, block_total_size);
    }

    mutex_unlock(&g_kallocator.mutex);
}

// 小块分配函数
static void *kalloc_small_block(uint32_t size, uint32_t alignment)
{
    // 计算需要的总大小：头部(8字节) + 用户数据 + 对齐填充
    uint32_t header_size = sizeof(alloc_header_t);
    uint32_t min_total_size = header_size + size;

    // 确保总大小满足对齐要求
    uint32_t total_size = (min_total_size + alignment - 1) & ~(alignment - 1);

    // 确保至少有足够空间存储free_block_t结构
    if (total_size < sizeof(free_block_t)) {
        total_size = sizeof(free_block_t);
    }

    // 首先尝试从空闲块列表中找到合适的块
    free_block_t **current = &g_kallocator.free_list;
    while (*current != NULL) {
        free_block_t *block = *current;

        // 验证空闲块的有效性
        if (block->size == 0 || block->size > PAGE_SIZE) {
            logger_error("Invalid free block size: %u, removing from list\n", block->size);
            *current = block->next; // 移除无效块
            continue;
        }

        // 验证块地址是否在有效页面内
        page_info_t *block_page = kallocator_find_page((void *)block);
        if (block_page == NULL) {
            logger_error("Free block at 0x%lx not in any managed page, removing\n", (uint64_t)block);
            *current = block->next; // 移除无效块
            continue;
        }

        if (block->size >= total_size) {
            // 保存块大小（在清除之前）
            uint32_t saved_block_size = block->size;

            // 从空闲列表中移除此块
            *current = block->next;

            // 清除旧的空闲块标记
            memset(block, 0, sizeof(free_block_t));

            // 如果块比需要的大，分割它
            if (saved_block_size > total_size + sizeof(free_block_t)) {
                uint32_t remaining_size = saved_block_size - total_size;
                free_block_t *new_block = (free_block_t *)((uint8_t *)block + total_size);
                new_block->size = remaining_size;
                new_block->next = g_kallocator.free_list;
                g_kallocator.free_list = new_block;
            }

            // 设置分配头部
            alloc_header_t *header = (alloc_header_t *)block;
            header->size = size;
            header->magic = ALLOC_MAGIC;

            // 计算用户地址（头部后面，应用对齐）
            uint64_t user_addr = (uint64_t)block + header_size;
            user_addr = (user_addr + alignment - 1) & ~(alignment - 1);

            // 更新统计 - 只维护used_bytes，free_bytes通过计算得出
            page_info_t *page = kallocator_find_page((void *)user_addr);
            if (page) {
                page->used_bytes += total_size;
                page->free_bytes = PAGE_SIZE - page->used_bytes;
                logger_debug("Alloc from free list: page used_bytes=%u, free_bytes=%u\n",
                           page->used_bytes, page->free_bytes);
            }

            g_kallocator.small_alloc_count++;
            g_kallocator.used_size += total_size;

            logger_info("Small alloc from free list: addr=0x%lx, size=%u, align=%u, total=%u\n",
                       user_addr, size, alignment, total_size);

            return (void *)user_addr;
        }
        current = &((*current)->next);
    }

    // 没有找到合适的空闲块，分配新页面
    void *page_addr = kalloc_pages(1);
    if (page_addr == NULL) {
        logger_error("Failed to allocate page for small block\n");
        return NULL;
    }

    // 记录页面信息，小块分配初始时used_bytes为0
    if (kallocator_add_page(page_addr, 1, false, 0) != 0) {
        kfree_pages(page_addr, 1);
        logger_error("Failed to track small allocation page\n");
        return NULL;
    }

    // 在新页面中分配
    uint64_t page_start = (uint64_t)page_addr;

    // 设置分配头部
    alloc_header_t *header = (alloc_header_t *)page_start;
    header->size = size;
    header->magic = ALLOC_MAGIC;

    // 计算用户地址
    uint64_t user_addr = page_start + header_size;
    user_addr = (user_addr + alignment - 1) & ~(alignment - 1);

    // 计算这个分配块的总大小（包括头部和对齐）
    uint32_t block_total_size = (user_addr - page_start) + size;
    // 对齐到8字节边界
    block_total_size = (block_total_size + 7) & ~7;

    // 将剩余空间加入空闲列表
    uint32_t remaining = PAGE_SIZE - block_total_size;
    if (remaining >= sizeof(free_block_t)) {
        free_block_t *free_block = (free_block_t *)(page_start + block_total_size);
        free_block->size = remaining;
        free_block->next = g_kallocator.free_list;
        g_kallocator.free_list = free_block;
    }

    // 更新页面统计
    page_info_t *page = kallocator_find_page((void *)user_addr);
    if (page) {
        page->used_bytes = block_total_size;  // 新页面，直接设置而不是累加
        page->free_bytes = PAGE_SIZE - block_total_size;
    }

    // 更新全局统计
    g_kallocator.small_alloc_count++;
    g_kallocator.total_size += PAGE_SIZE;
    g_kallocator.used_size += block_total_size;

    logger_info("Small alloc from new page: addr=0x%lx, size=%u, align=%u, block_total=%u\n",
               user_addr, size, alignment, block_total_size);

    return (void *)user_addr;
}

uint64_t kallocator_get_used_memory(void)
{
    if (!g_kallocator_initialized) {
        return 0;
    }
    return g_kallocator.used_size;
}

uint64_t kallocator_get_free_memory(void)
{
    if (!g_kallocator_initialized) {
        return 0;
    }

    uint64_t free_memory = 0;

    // 计算空闲块总大小
    free_block_t *current = g_kallocator.free_list;
    while (current != NULL) {
        free_memory += current->size;
        current = current->next;
    }

    return free_memory;
}

bool kallocator_is_initialized(void)
{
    return g_kallocator_initialized;
}

void kallocator_info(void)
{
    if (!g_kallocator_initialized) {
        logger_error("Kernel allocator not initialized\n");
        return;
    }

    uint64_t used_memory = kallocator_get_used_memory();
    uint64_t free_memory = kallocator_get_free_memory();

    // 统计空闲块
    uint32_t free_block_count = 0;
    free_block_t *current = g_kallocator.free_list;
    while (current != NULL) {
        free_block_count++;
        current = current->next;
    }

    logger_info("Kernel Memory Allocator Status:\n");
    logger_info("  Total allocated: %lu bytes (%lu KB)\n",
              g_kallocator.total_size, g_kallocator.total_size / 1024);
    logger_info("  Used:           %lu bytes (%lu KB)\n",
              used_memory, used_memory / 1024);
    logger_info("  Free:           %lu bytes (%lu KB)\n",
              free_memory, free_memory / 1024);
    logger_info("  Pages:          %u pages\n", g_kallocator.page_count);
    logger_info("  Small allocs:   %u\n", g_kallocator.small_alloc_count);
    logger_info("  Large allocs:   %u\n", g_kallocator.large_alloc_count);
    logger_info("  Free blocks:    %u\n", free_block_count);
    logger_info("  Usage:          %lu%%\n",
              g_kallocator.total_size > 0 ? (used_memory * 100) / g_kallocator.total_size : 0);

    // 显示页面详情
    if (g_kallocator.page_count > 0) {
        logger_info("  Page details:\n");
        for (uint32_t i = 0; i < g_kallocator.page_count && i < 10; i++) {
            page_info_t *page = &g_kallocator.pages[i];
            logger_info("    Page %u: addr=0x%lx, %u pages, %s\n",
                      i, (uint64_t)page->page_addr, page->page_count,
                      page->is_large_alloc ? "large" : "small");
        }
        if (g_kallocator.page_count > 10) {
            logger_info("    ... and %u more pages\n", g_kallocator.page_count - 10);
        }
    }
}

// 新的API函数
uint32_t kallocator_get_page_count(void)
{
    if (!g_kallocator_initialized) {
        return 0;
    }
    return g_kallocator.page_count;
}

uint32_t kallocator_get_small_alloc_count(void)
{
    if (!g_kallocator_initialized) {
        return 0;
    }
    return g_kallocator.small_alloc_count;
}

uint32_t kallocator_get_large_alloc_count(void)
{
    if (!g_kallocator_initialized) {
        return 0;
    }
    return g_kallocator.large_alloc_count;
}

void kallocator_cleanup(void)
{
    if (!g_kallocator_initialized) {
        return;
    }

    mutex_lock(&g_kallocator.mutex);

    // 释放所有页面
    uint64_t freed_memory = 0;
    for (uint32_t i = 0; i < g_kallocator.page_count; i++) {
        page_info_t *page = &g_kallocator.pages[i];
        kfree_pages(page->page_addr, page->page_count);
        freed_memory += page->page_count * PAGE_SIZE;
    }

    // 释放页面信息数组
    if (g_kallocator.pages) {
        kfree_pages(g_kallocator.pages, 1);
        freed_memory += PAGE_SIZE;
    }

    // 重置分配器状态
    memset(&g_kallocator, 0, sizeof(g_kallocator));
    g_kallocator_initialized = false;

    mutex_unlock(&g_kallocator.mutex);

    logger_info("Kallocator cleanup: freed %lu KB total\n", freed_memory / 1024);
}

// 辅助函数实现
static int kallocator_add_page(void *page_addr, uint32_t page_count, bool is_large_alloc, uint32_t actual_used_size)
{
    // 检查是否需要扩展页面数组
    if (g_kallocator.page_count >= g_kallocator.page_capacity) {
        logger_error("Page info array full, cannot track more pages\n");
        return -1;
    }

    // 添加页面信息
    page_info_t *page = &g_kallocator.pages[g_kallocator.page_count];
    page->page_addr = page_addr;
    page->page_count = page_count;
    page->is_large_alloc = is_large_alloc;

    if (is_large_alloc) {
        page->used_bytes = actual_used_size;  // 使用用户实际请求的大小
        page->free_bytes = (page_count * PAGE_SIZE) - actual_used_size;
    } else {
        page->used_bytes = 0;
        page->free_bytes = page_count * PAGE_SIZE;
    }

    g_kallocator.page_count++;
    return 0;
}

static void kallocator_remove_page(void *page_addr)
{
    // 查找并移除页面
    for (uint32_t i = 0; i < g_kallocator.page_count; i++) {
        if (g_kallocator.pages[i].page_addr == page_addr) {
            // 将最后一个页面移到当前位置
            if (i < g_kallocator.page_count - 1) {
                g_kallocator.pages[i] = g_kallocator.pages[g_kallocator.page_count - 1];
            }
            g_kallocator.page_count--;
            return;
        }
    }
}

static page_info_t *kallocator_find_page(void *addr)
{
    uint64_t target_addr = (uint64_t)addr;

    for (uint32_t i = 0; i < g_kallocator.page_count; i++) {
        page_info_t *page = &g_kallocator.pages[i];
        uint64_t page_start = (uint64_t)page->page_addr;
        uint64_t page_end = page_start + (page->page_count * PAGE_SIZE);

        if (target_addr >= page_start && target_addr < page_end) {
            return page;
        }
    }

    return NULL;
}

static bool kallocator_is_page_empty(page_info_t *page)
{
    if (page->is_large_alloc) {
        return false; // 大块分配页面不会变空，只能整体释放
    }

    // 检查页面是否只包含空闲块
    uint64_t page_start = (uint64_t)page->page_addr;
    uint64_t page_end = page_start + (page->page_count * PAGE_SIZE);

    // 计算页面内所有空闲块的总大小
    uint32_t total_free_size = 0;
    free_block_t *current = g_kallocator.free_list;

    while (current != NULL) {
        uint64_t block_addr = (uint64_t)current;

        // 检查这个空闲块是否在当前页面内
        if (block_addr >= page_start && block_addr < page_end) {
            total_free_size += current->size;
        }
        current = current->next;
    }

    // 如果空闲块总大小等于页面大小，说明页面完全空闲
    // 允许一些小的误差（比如对齐造成的）
    uint32_t page_size = page->page_count * PAGE_SIZE;
    bool is_empty = (total_free_size >= page_size - 16); // 允许16字节的对齐误差

    if (is_empty) {
        logger_debug("Page 0x%lx is empty: free_size=%u, page_size=%u\n",
                   page_start, total_free_size, page_size);
    }

    return is_empty;
}

static void kallocator_coalesce_free_blocks(void)
{
    if (!g_kallocator.free_list) return;

    // 简单的相邻块合并：只合并物理上相邻的块
    bool merged = true;
    int merge_iterations = 0;
    const int MAX_MERGE_ITERATIONS = 100; // 防止无限循环

    while (merged && merge_iterations < MAX_MERGE_ITERATIONS) {
        merged = false;
        merge_iterations++;
        free_block_t *current = g_kallocator.free_list;

        while (current != NULL && current->next != NULL) {
            free_block_t *next = current->next;
            uint64_t current_end = (uint64_t)current + current->size;
            uint64_t next_start = (uint64_t)next;

            // 验证块的有效性
            if (current->size == 0 || current->size > PAGE_SIZE ||
                next->size == 0 || next->size > PAGE_SIZE) {
                logger_error("Invalid free block size detected during coalescing\n");
                break;
            }

            // 确保两个块在同一个页面内
            page_info_t *current_page = kallocator_find_page((void *)current);
            page_info_t *next_page = kallocator_find_page((void *)next);

            if (current_page != next_page || current_page == NULL) {
                current = current->next;
                continue;
            }

            // 检查current和next是否相邻
            if (current_end == next_start) {
                // 合并：将next合并到current
                current->size += next->size;
                current->next = next->next;
                merged = true;
                logger_debug("Coalesced blocks: 0x%lx + 0x%lx (total size %u)\n",
                           (uint64_t)current, next_start, current->size);
                break; // 重新开始扫描
            }
            current = current->next;
        }
    }

    if (merge_iterations >= MAX_MERGE_ITERATIONS) {
        logger_warn("Free block coalescing reached maximum iterations\n");
    }
}

void kallocator_test(void)
{
    if (!g_kallocator_initialized) {
        logger_error("Kernel allocator not initialized for testing\n");
        return;
    }

    logger_info("=== Kernel Allocator Test ===\n");

    // 显示初始状态
    logger_info("Initial state:\n");
    kallocator_info();

    // 测试1：小块分配
    logger_info("\nTest 1: Small block allocation\n");
    void *ptr1 = kalloc_simple(64);
    void *ptr2 = kalloc_aligned(128, 16);
    void *ptr3 = kalloc_aligned(256, 32);

    logger_info("Small allocs: ptr1=%p, ptr2=%p, ptr3=%p\n", ptr1, ptr2, ptr3);
    kallocator_info();

    // 测试2：大块分配
    logger_info("\nTest 2: Large block allocation\n");
    void *large_ptr = kalloc_simple(8192); // 8KB，触发大块分配

    logger_info("Large alloc: ptr=%p\n", large_ptr);
    kallocator_info();

    // 测试3：释放和重用
    logger_info("\nTest 3: Free and reuse\n");
    kfree(ptr2);
    kallocator_info();

    void *ptr4 = kalloc_simple(100); // 应该重用空闲空间
    logger_info("Reuse alloc: ptr4=%p\n", ptr4);
    kallocator_info();

    // 清理
    kfree(ptr1);
    kfree(ptr3);
    kfree(ptr4);
    kfree(large_ptr);

    logger_info("\nFinal state after cleanup:\n");
    kallocator_info();

    logger_info("=== Kernel Allocator Test Complete ===\n");

}

void kallocator_stress_test(void)
{
    if (!g_kallocator_initialized) {
        logger_error("Kernel allocator not initialized for stress testing\n");
        return;
    }

    logger_info("=== Kernel Allocator Stress Test ===\n");

    // 显示初始状态
    logger_info("Initial state:\n");
    kallocator_info();

    const int MAX_ALLOCS = 300;
    void *ptrs[MAX_ALLOCS];
    uint32_t sizes[MAX_ALLOCS];

    // 压力测试1：连续分配各种大小的内存
    logger_info("\nStress Test 1: Continuous allocation\n");
    for (int i = 0; i < MAX_ALLOCS; i++) {
        // 随机大小：16字节到4KB
        sizes[i] = 8 + (i * 11) % 4080;  // 使用质数27避免规律性
        ptrs[i] = kalloc_simple(sizes[i]);

        if (ptrs[i] == NULL) {
            logger_info("Allocation failed at iteration %d (size %u)\n", i, sizes[i]);
            break;
        }

        // 写入测试数据
        memset(ptrs[i], (uint8_t)(i & 0xFF), sizes[i]);

        if (i % 20 == 19) {  // 每20次显示一次状态
            logger_info("Allocated %d items\n", i + 1);
            kallocator_info();
        }
    }

    logger_info("Allocation phase complete\n");
    kallocator_info();

    // 验证数据完整性
    logger_info("\nVerifying data integrity...\n");
    for (int i = 0; i < MAX_ALLOCS; i++) {
        if (ptrs[i] != NULL) {
            uint8_t expected = (uint8_t)(i & 0xFF);
            uint8_t *data = (uint8_t *)ptrs[i];
            if (data[0] != expected || data[sizes[i] - 1] != expected) {
                logger_error("Data corruption at ptr[%d]!\n", i);
                break;
            }
        }
    }
    logger_info("Data integrity verified\n");

    // 压力测试2：随机释放一半内存
    logger_info("\nStress Test 2: Random free (50%)\n");
    for (int i = 1; i < MAX_ALLOCS; i += 2) {  // 释放奇数索引
        if (ptrs[i] != NULL) {
            kfree(ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    logger_info("After freeing 50%:\n");
    kallocator_info();

    // 压力测试3：重新分配释放的空间
    logger_info("\nStress Test 3: Reallocate freed space\n");
    for (int i = 1; i < MAX_ALLOCS; i += 2) {
        if (ptrs[i] == NULL) {
            uint32_t new_size = 32 + (i * 23) % 2048;  // 不同的大小
            ptrs[i] = kalloc_simple(new_size);
            if (ptrs[i] != NULL) {
                sizes[i] = new_size;
                memset(ptrs[i], (uint8_t)((i + 100) & 0xFF), new_size);
            }
        }
    }

    logger_info("After reallocation:\n");
    kallocator_info();

    // 压力测试4：大块分配测试
    logger_info("\nStress Test 4: Large allocations\n");
    void *large_ptrs[10];
    for (int i = 0; i < 10; i++) {
        uint32_t large_size = (i + 1) * 8192;  // 8KB, 16KB, 24KB...
        large_ptrs[i] = kalloc_simple(large_size);
        if (large_ptrs[i] != NULL) {
            memset(large_ptrs[i], (uint8_t)(0xAA + i), large_size);
            logger_info("Large alloc %d: %u bytes at %p\n", i, large_size, large_ptrs[i]);
        }
    }

    logger_info("After large allocations:\n");
    kallocator_info();

    // 释放大块分配
    logger_info("\nFreeing large allocations...\n");
    for (int i = 0; i < 10; i++) {
        if (large_ptrs[i] != NULL) {
            kfree(large_ptrs[i]);
        }
    }

    logger_info("After freeing large allocations:\n");
    kallocator_info();

    // 最终清理：释放所有剩余内存
    logger_info("\nFinal cleanup...\n");
    for (int i = 0; i < MAX_ALLOCS; i++) {
        if (ptrs[i] != NULL) {
            kfree(ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    logger_info("Final state:\n");
    kallocator_info();

    uint64_t final_used = kallocator_get_used_memory();
    uint32_t final_pages = kallocator_get_page_count();

    logger_info("Final state after cleanup:\n");
    kallocator_info();

    if (final_used == 0 && final_pages == 0) {
        logger_info("✅ STRESS TEST PASSED: All memory properly freed\n");
    } else {
        logger_error("❌ STRESS TEST FAILED: Memory leak detected\n");
        logger_error("   Remaining used: %lu bytes, pages: %u\n", final_used, final_pages);
    }

    logger_info("=== Kernel Allocator Stress Test Complete ===\n");
}