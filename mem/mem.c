
#include "mem/mem.h"
#include "os_cfg.h"
#include "task/mutex.h"
#include "io.h"
#include "thread.h"
#include "mem/bitmap.h"
#include "mem/page.h"
#include "mem/mmu.h"
#include "mem/aj_string.h"


#define GET_PGD_INDEX(addr) ((addr >> 39) & 0x1ff)
#define GET_PUD_INDEX(addr) ((addr >> 30) & 0x1ff)
#define GET_PMD_INDEX(addr) ((addr >> 21) & 0x1ff)
#define GET_PTE_INDEX(addr) ((addr >> 12) & 0x1ff)

// 向上对齐到 bound 边界
#define UP2(size, bound)   (((size) + (bound) - 1) & ~((bound) - 1))

// 向下对齐到 bound 边界
#define DOWN2(size, bound) ((size) & ~((bound) - 1))

// 内核虚拟地址和物理地址转换
#define PHYS_TO_VIRT(pa) ((void *)((pa) + 0)) // 替换成你实际的转换

#define assert(condition) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: %s, function %s, file %s, line %d\n", \
                    #condition, __func__, __FILE__, __LINE__); \
            while(1); \
        } \
    } while(0)

static addr_alloc_t g_alloc;

static pte_t kpage_dir;

void mark_kernel_memory_allocated(uint64_t heap_start) {
    uint64_t start = KERNEL_RAM_START;
    uint64_t end = heap_start;

    // 确保起始地址小于结束地址
    if (start >= end) {
        return;
    }

    // 如果 heap_start 不是页对齐的，将其向上对齐
    if (end % g_alloc.page_size != 0) {
        end = (end + g_alloc.page_size - 1) & ~(g_alloc.page_size - 1);
    }

    // 计算起始和结束页索引
    uint64_t start_page = (start - g_alloc.start) / g_alloc.page_size;
    uint64_t end_page = (end - g_alloc.start) / g_alloc.page_size;

    // 遍历所有页，并在bitmap中标记为已分配
    for (uint64_t i = start_page; i <= end_page; i++) {
        bitmap_set(&g_alloc.bitmap, i);
    }

    printf("Marked memory from 0x%lx to 0x%lx as allocated.\n", start, end);
}

void alloctor_init() 
{
    bitmap_init(&g_alloc.bitmap, bitmap_buffer, OS_CFG_BITMAP_SIZE);
    g_alloc.start = KERNEL_RAM_START;
    g_alloc.size = KERNEL_RAM_SIZE;
    g_alloc.page_size = PAGE_SIZE;

    mutex_init(&g_alloc.mutex);

    uint64_t heap_start = (uint64_t)(void*)__heap_flag + 0x900000;
    printf("heap start: 0x%x\n", heap_start);
    mark_kernel_memory_allocated(heap_start);
}




static uint64_t addr_alloc_page(addr_alloc_t *alloc, int page_count) {
    uint64_t addr = 0;
    mutex_lock(&alloc->mutex);

    int page_index = bitmap_find_contiguous_free(&alloc->bitmap, page_count);
    if (page_index >= 0) {
        // printf("Allocating pages starting from index: %d\n", page_index);
        bitmap_set_range(&alloc->bitmap, page_index, page_count);
        addr = alloc->start + page_index * alloc->page_size;
    }
    // 调试输出，确认是否设置了位图
    if (bitmap_test(&alloc->bitmap, page_index)) {
        // printf("Page index %d marked as allocated.\n", page_index);
    }

    mutex_unlock(&alloc->mutex);
    return addr;
}

static void addr_free_page(addr_alloc_t *alloc, uint64_t addr, int page_count) {
    mutex_lock(&alloc->mutex);
    
    static uint64_t heap_start = (uint64_t)(void*)__heap_flag + 0x900000;
    heap_start = UP2(PAGE_SIZE, heap_start);
    // 确保释放的页不在内核空间内（内核空间地址范围：KERNEL_RAM_START 到 heap_start）
    assert( addr >= heap_start);

    uint64_t pg_idx = (addr - alloc->start) / alloc->page_size;
    bitmap_clear_range(&alloc->bitmap, pg_idx, page_count);

    mutex_unlock(&alloc->mutex);
}

// ============= 内核内存分配释放 ================

void * kalloc_page() {
    return (void*)addr_alloc_page(&g_alloc, 1);
}

void kfree_page(void *addr) {
    addr_free_page(&g_alloc, (uint64_t)addr, 1);
}


// ============= 用户程序内存分配释放 =================

pte_t *find_pte(pte_t *page_dir, uint64_t vaddr, int alloc) {

    printf("find_pte called for vaddr: 0x%lx\n", vaddr);

    // 获取PGD索引
    pte_t *pgd = &page_dir[GET_PGD_INDEX(vaddr)];
    printf("    PGD Index: %d, PGD entry: 0x%lx\n", GET_PGD_INDEX(vaddr), pgd->pte);

    // 分配 PUD
    if (!pgd->table.is_valid) {
        
        if (!alloc) return NULL;
        printf("    PGD entry is not valid, allocating PUD\n");
        uint64_t pud_phys = addr_alloc_page(&g_alloc, 1);
        if (!pud_phys) return NULL;

        pgd->pte = (pud_phys >> 12) << 12 | 0x3; // valid + table
        printf("    Allocated PUD at: 0x%lx, setting PGD entry to: 0x%lx\n", pud_phys, pgd->pte);
        memset((void *)(pud_phys), 0, 0x1000);
    }

    // 获取PUD表项
    pte_t *pud = (pte_t *)(void*)(uint64_t)((pgd->table.next_table_addr) << 12ULL);
    printf("    PUD entry: 0x%lx\n", pud->pte);

    pte_t *pud_entry = &pud[GET_PUD_INDEX(vaddr)];
    printf("    PUD Index: %d, PUD entry: 0x%lx\n", GET_PUD_INDEX(vaddr), pud_entry->pte);

    // 分配 PMD
    if (!pud_entry->table.is_valid) {
        
        if (!alloc) return NULL;
        printf("    PUD entry is not valid, allocating PMD\n");
        uint64_t pmd_phys = addr_alloc_page(&g_alloc, 1);
        if (!pmd_phys) return NULL;

        pud_entry->pte = (pmd_phys >> 12) << 12 | 0x3;
        printf("    Allocated PMD at: 0x%lx, setting PUD entry to: 0x%lx\n", pmd_phys, pud_entry->pte);
        memset((void *)(pmd_phys), 0, 0x1000);
    }

    // 获取PMD表项
    pte_t *pmd = (pte_t *)(void*)(uint64_t)((pud_entry->table.next_table_addr) << 12ULL);
    printf("    PMD entry: 0x%lx\n", pmd->pte);

    pte_t *pmd_entry = &pmd[GET_PMD_INDEX(vaddr)];
    printf("    PMD Index: %d, PMD entry: 0x%lx\n", GET_PMD_INDEX(vaddr), pmd_entry->pte);

    // 分配 Page Table
    if (!pmd_entry->table.is_valid) {
        
        if (!alloc) return NULL;
        printf("    PMD entry is not valid, allocating Page Table\n");
        uint64_t pt_phys = addr_alloc_page(&g_alloc, 1);
        if (!pt_phys) return NULL;

        pmd_entry->pte = (pt_phys >> 12) << 12 | 0x3;
        printf("    Allocated Page Table at: 0x%lx, setting PMD entry to: 0x%lx\n", pt_phys, pmd_entry->pte);
        memset((void *)(pt_phys), 0, 0x1000);
    }

    // 获取PTE表项
    pte_t *pte_base = (pte_t *)(void*)(uint64_t)((pmd_entry->table.next_table_addr) << 12ULL);
    printf("    PTE Index: %d, PTE entry: 0x%lx\n", GET_PTE_INDEX(vaddr), pte_base[GET_PTE_INDEX(vaddr)].pte);

    return &pte_base[GET_PTE_INDEX(vaddr)];
}

int memory_create_map(pte_t *page_dir, uint64_t vaddr, uint64_t paddr, int count, uint64_t perm) {
    printf("Starting memory_create_map for vaddr 0x%lx, paddr 0x%lx, count %d\n", vaddr, paddr, count);
    
    for (int i = 0; i < count; i++) {
        // 获取对应的 PTE
        pte_t *pte_entry = find_pte(page_dir, vaddr, 1);
        if (pte_entry == NULL) {
            printf("memory_create_map: Failed to find or allocate PTE for vaddr 0x%lx\n", vaddr);
            return -1;
        }

        if (pte_entry->l3_page.is_valid) {
            printf("memory_create_map: vaddr 0x%lx is already mapped to pfn 0x%lx\n", vaddr, pte_entry->l3_page.pfn);
            return -1;
        }

        // 打印分配信息
        printf("Mapping vaddr 0x%lx to paddr 0x%lx\n", vaddr, paddr);
        
        // 设置 PTE 为有效并设置物理地址
        pte_entry->l3_page.is_valid = 1;
        pte_entry->l3_page.is_table = 0;
        pte_entry->l3_page.pfn = (paddr >> 12) & 0xFFFFFFFFF; // 36 bits PFN Page Frame Number

        // 设置权限
        pte_entry->l3_page.AF = 1;
        pte_entry->l3_page.SH = 3; // Inner shareable
        pte_entry->l3_page.AP = 1;
        pte_entry->l3_page.UXN = 0;
        pte_entry->l3_page.PXN = 1;
        pte_entry->l3_page.attr_index = 0; // Normal memory

        // 输出映射后的权限和地址信息
        printf("Mapped PTE entry: is_valid=%d, pfn=0x%lx, AF=%d, SH=%d, AP=%d, UXN=%d, PXN=%d, attr_index=%d\n",
               pte_entry->l3_page.is_valid, pte_entry->l3_page.pfn, pte_entry->l3_page.AF, pte_entry->l3_page.SH,
               pte_entry->l3_page.AP, pte_entry->l3_page.UXN, pte_entry->l3_page.PXN, pte_entry->l3_page.attr_index);

        // 更新虚拟地址和物理地址
        vaddr += PAGE_SIZE;
        paddr += PAGE_SIZE;
    }

    return 0;
}

pte_t * current_page_dir() {
    return (pte_t *)read_ttbr0_el1();
}

uint64_t memory_get_paddr(pte_t * page_dir, uint64_t vaddr) {
    
    pte_t * pte = find_pte(page_dir, vaddr, 0);
    
    if (pte == (pte_t *)0) {
        return 0;
    }

    return (pte->l3_page.pfn << 12) + (vaddr & (PAGE_SIZE - 1));
}

uint64_t memory_alloc_page(pte_t * page_dir, uint64_t vaddr, uint64_t size, int perm) {
    uint64_t curr_vaddr = vaddr;
    int page_count = UP2(size, PAGE_SIZE) / PAGE_SIZE;
    vaddr = DOWN2(vaddr, PAGE_SIZE);

    // 逐页分配内存，然后建立映射关系
    for (int i = 0; i < page_count; i++) {
        // 分配需要的内存
        uint64_t paddr = addr_alloc_page(&g_alloc, 1);
        if (paddr == 0) {
            printf("mem alloc failed. no memory");
            return -1;
        }

        // 建立分配的内存与指定地址的关联
        int err = memory_create_map((pte_t *)page_dir, curr_vaddr, paddr, 1, perm);
        if (err < 0) {
            printf("create memory map failed. err = %d", err);
            addr_free_page(&g_alloc, vaddr, i + 1);
            return -1;
        }

        curr_vaddr += PAGE_SIZE;
    }

    return 0;
}

void memory_free_page (pte_t * page_dir, uint64_t addr) {
    
    pte_t * pte = find_pte(page_dir, addr, 0);

    addr_free_page(&g_alloc, (pte->l3_page.pfn << 12), 1);

    pte->pte = 0;
}


pte_t * create_uvm (void) {
    pte_t * page_dir = (pte_t *)(void*)addr_alloc_page(&g_alloc, 1);
    if (page_dir == 0) {
        return 0;
    }
    memset((void *)page_dir, 0, PAGE_SIZE);

    //TODO: 这个地方需要让el0进程共享内核空间 

    return page_dir;
}

void _destroy_page_table_vm(pte_t *table, int level) {
    // 输出当前正在处理的层级
    printf("Destroying page table at level %d\n", level);
    
    if (level >= 4) return;

    // 各级页表的最大项数（按实际情况调整）
    static const int max_entries[] = {1, 4, 512, 512};
    int entry_count = max_entries[level];

    // 遍历当前层级的所有页表项
    for (int i = 0; i < entry_count; i++) {
        pte_t *entry = &table[i];

        if (!entry->table.is_valid)
            continue;

        uint64_t next_table_phys = entry->table.next_table_addr << 12;
        void *next_table = (void *)next_table_phys;

        // 输出当前页表项的信息
        printf("Level %d, Entry %d: is_valid = %d, is_table = %d, Next Table Address = 0x%lx\n",
               level, i, entry->table.is_valid, entry->l3_page.is_table, next_table_phys);

        if (level == 3) {
            // PTE 层：释放实际映射的物理页
            uint64_t page_phys = entry->l3_page.pfn << 12;
            printf("Level %d, Freeing physical page: 0x%lx\n", level, page_phys);
            addr_free_page(&g_alloc, page_phys, 1);
        } else {
            // 递归释放下一层页表
            _destroy_page_table_vm((pte_t *)next_table, level + 1);
        }

        // 释放当前这一级的页表页
        if (entry->l3_page.is_table == 1) {
            printf("Level %d, Freeing page table at entry %d: 0x%lx\n", level, i, next_table_phys);
            addr_free_page(&g_alloc, next_table_phys, 1);
        }
    }
}

void _destroy_page_table(pte_t *table, int level) {
    // 输出当前正在处理的层级
    printf("Destroying page table at level %d\n", level);
    
    if (level >= 4) return;

    // 各级页表的最大项数（按实际情况调整）
    static const int max_entries[] = {1, 4, 512, 512};
    int entry_count = max_entries[level];

    // 遍历当前层级的所有页表项
    for (int i = 0; i < entry_count; i++) {
        pte_t *entry = &table[i];

        if (!entry->table.is_valid)
            continue;

        uint64_t next_table_phys = entry->table.next_table_addr << 12;
        void *next_table = (void *)next_table_phys;

        // 输出当前页表项的信息
        printf("Level %d, Entry %d: is_valid = %d, is_table = %d, Next Table Address = 0x%lx\n",
               level, i, entry->table.is_valid, entry->l3_page.is_table, next_table_phys);
        
        // 递归释放下一层页表
        _destroy_page_table((pte_t *)next_table, level + 1);

        // 释放当前这一级的页表页
        if (entry->l3_page.is_table == 1) {
            printf("Level %d, Freeing page table at entry %d: 0x%lx\n", level, i, next_table_phys);
            addr_free_page(&g_alloc, next_table_phys, 1);
        }
    }
}

bool _copy_page_table(pte_t *src_table, pte_t *dst_table, int level) {
    for (int i = 0; i < 512; i++) {
        pte_t *src_entry = &src_table[i];
        if (!src_entry->table.is_valid)
            continue;

        if (level == 3) {
            // 第4级页表：实际映射的物理页
            uint64_t src_phys = src_entry->l3_page.pfn << 12;
            uint64_t dst_phys = addr_alloc_page(&g_alloc, 1);
            if (!dst_phys) return false;

            // 拷贝页内容
            memcpy((void *)dst_phys, (void *)src_phys, PAGE_SIZE);

            // 设置目标页表项
            dst_table[i].pte = (dst_phys >> 12 << 12) | (src_entry->pte & 0xFFF);
        } else {
            // 中间层级：创建目标子表并递归拷贝
            uint64_t src_next_phys = src_entry->table.next_table_addr << 12;
            pte_t *src_next = (pte_t *)src_next_phys;

            uint64_t dst_next_phys = addr_alloc_page(&g_alloc, 1);
            if (!dst_next_phys) return false;

            pte_t *dst_next = (pte_t *)dst_next_phys;
            memset(dst_next, 0, PAGE_SIZE);

            // 设置当前页表项指向新分配的页表
            dst_table[i].pte = (dst_next_phys >> 12 << 12) | (src_entry->pte & 0xFFF);

            if (!_copy_page_table(src_next, dst_next, level + 1))
                return false;
        }
    }
    return true;
}

void destroy_uvm_4level(pte_t *page_dir) {
    // level: 0 = PGD, 1 = PUD, 2 = PMD, 3 = PTE
    _destroy_page_table_vm(page_dir, 0);
    addr_free_page(&g_alloc, (uint64_t)page_dir, 1);  // 最后释放 PGD 自身
}

void destory_4level(pte_t *page_dir) {
    // level: 0 = PGD, 1 = PUD, 2 = PMD, 3 = PTE
    _destroy_page_table(page_dir, 0);
    addr_free_page(&g_alloc, (uint64_t)page_dir, 1);  // 最后释放 PGD 自身
}

// 内核将数据拷贝到指定进程空间下
void copydata_to_uvm(pte_t * page_dir, uint64_t vaddr, uint64_t paddr, uint64_t size) {

    uint64_t offset = 0;
    while (offset < size) {
        uint64_t curr_vaddr = vaddr + offset;

        // 获取页表项
        pte_t *pte = find_pte(page_dir, curr_vaddr, 0);
        if (!pte || pte->l3_page.is_valid == 0) {
            // 页未映射，直接跳过或报错
            printf("No valid mapping for vaddr 0x%lx\n", curr_vaddr);
            return;
        }

        // 获取物理页帧号（PFN），并计算出物理地址
        uint64_t page_pfn = pte->l3_page.pfn;
        uint64_t page_paddr = page_pfn << 12;

        // 虚拟页起始地址（用于写入）
        uint8_t *dest = (uint8_t *)PHYS_TO_VIRT(page_paddr);

        // 当前页剩余空间
        uint64_t page_offset = curr_vaddr & (PAGE_SIZE - 1);
        uint64_t page_remain = PAGE_SIZE - page_offset;
        uint64_t copy_len = (size - offset > page_remain) ? page_remain : (size - offset);

        // 源地址
        uint8_t *src = (uint8_t *)PHYS_TO_VIRT(paddr + offset);

        // 拷贝数据
        memcpy(dest + page_offset, src, copy_len);

        offset += copy_len;
    }
}

// 复制某个进程空间的所有内存到另一个进程空间下
int memory_copy_uvm_4level(pte_t * dst_pgd, pte_t *src_pgd) {

    if (!_copy_page_table(src_pgd, dst_pgd, 0)) {
        destroy_uvm_4level(dst_pgd);
        return -1;
    }

    return 0;
}




uint64_t mutex_test_num = 6;

uint64_t mutex_test_add() {
    mutex_lock(&g_alloc.mutex);
    for (int i=0; i<10000; i++) {
        mutex_test_num ++;
        mutex_test_num --;
        mutex_test_num ++;
        mutex_test_num --;
        mutex_test_num ++;
        mutex_test_num --;
        mutex_test_num ++;
        mutex_test_num --;
    }
    mutex_unlock(&g_alloc.mutex);
    return mutex_test_num;
}

uint64_t mutex_test_minus() {
    mutex_lock(&g_alloc.mutex);
    for (int i=0; i<10000; i++) {
        mutex_test_num ++;
        mutex_test_num --;
        mutex_test_num ++;
        mutex_test_num --;
        mutex_test_num ++;
        mutex_test_num --;
        mutex_test_num ++;
        mutex_test_num --;
    }
    mutex_unlock(&g_alloc.mutex);
    return mutex_test_num;
}

void mutex_test_print() {
    mutex_lock(&g_alloc.mutex);
    printf("mutex_test_num = %d, current task: %d\n", mutex_test_num, ((tcb_t*)(void*)read_tpidr_el0())->id);
    mutex_test_num = 6;
    mutex_unlock(&g_alloc.mutex);
}


// 获取系统当前可用的总页数
static int get_available_page_count(addr_alloc_t *alloc) {
    int available_page_count = 0;
    
    // 锁住分配器，确保线程安全
    mutex_lock(&alloc->mutex);
    
    // 获取位图的总页数
    uint64_t start_page = (0x40000000 - g_alloc.start) / g_alloc.page_size;
    uint64_t end_page = (0x60000000 - g_alloc.start) / g_alloc.page_size;
    
    // 遍历位图，统计未分配的页面数量
    for (int i = start_page; i < end_page; i++) {
        if (!bitmap_test(&alloc->bitmap, i)) {
            available_page_count++;
        }
    }

    // 解锁
    mutex_unlock(&alloc->mutex);
    
    return available_page_count;
}

void assert_bitmap_state(uint64_t addr, uint8_t expected_state) {
    // 计算页索引，相对于内存池的偏移量
    size_t page_index = (addr - g_alloc.start) / PAGE_SIZE;

    // 获取位图的当前状态
    uint8_t current_state = bitmap_test(&g_alloc.bitmap, page_index);

    // 输出调试信息
    printf("Checking page at address: 0x%lx, expected state: %d, current state: %d\n", addr, expected_state, current_state);

    // 检查当前状态与预期状态是否匹配
    assert(current_state == expected_state);
}

void test_alloc_free() {
    uint64_t addr;

    // 测试：分配1页
    addr = addr_alloc_page(&g_alloc, 1);
    printf("Allocated 1 page at address: 0x%lx\n", addr);
    assert(addr != 0);

    // 确认分配后的地址在位图中标记为已使用
    assert_bitmap_state(addr, 1);

    // 测试：释放1页
    addr_free_page(&g_alloc, addr, 1);
    printf("Freed 1 page at address: 0x%lx\n", addr);

    // 确认释放后的地址在位图中标记为未使用
    assert_bitmap_state(addr, 0);

    // 测试：分配多页
    addr = addr_alloc_page(&g_alloc, 4);  // 假设连续4页
    printf("Allocated 4 pages starting at address: 0x%lx\n", addr);
    assert(addr != 0);

    // 确认分配的4页在位图中标记为已使用
    for (int i = 0; i < 4; i++) {
        assert_bitmap_state(addr + i * PAGE_SIZE, 1);
    }

    // 测试：释放多页
    addr_free_page(&g_alloc, addr, 4);
    printf("Freed 4 pages starting at address: 0x%lx\n", addr);

    // 确认释放后的4页在位图中标记为未使用
    for (int i = 0; i < 4; i++) {
        assert_bitmap_state(addr + i * PAGE_SIZE, 0);
    }
}

void test_find_free_page() {
    uint64_t addr;

    // 测试：找到第一个空闲页
    addr = addr_alloc_page(&g_alloc, 1);
    printf("Allocated 1 page at address: 0x%lx\n", addr);
    assert(addr != 0);

    // 测试：查找第一个空闲页
    uint64_t free_page = bitmap_find_first_free(&g_alloc.bitmap);
    printf("First free page is at index: %lu\n", free_page);
    assert(free_page != -1);

    // 确认返回的空闲页是正确的
    assert_bitmap_state(free_page * PAGE_SIZE + g_alloc.start, 0);

    addr_free_page(&g_alloc, addr, 1);
}

void test_find_contiguous_free_pages() {
    uint64_t addr;

    // 测试：找到连续的空闲页
    addr = addr_alloc_page(&g_alloc, 4);  // 分配4页
    printf("Allocated 4 pages at address: 0x%lx\n", addr);
    assert(addr != 0);

    // 测试：查找4个连续的空闲页
    uint64_t free_page = bitmap_find_contiguous_free(&g_alloc.bitmap, 4);
    printf("Found contiguous 4 free pages at index: %lu\n", free_page);
    assert(free_page != -1);

    // 确认返回的连续空闲页是正确的
    for (int i = 0; i < 4; i++) {
        assert_bitmap_state((free_page + i) * PAGE_SIZE + g_alloc.start, 0);
    }

    addr_free_page(&g_alloc, addr, 4);
}

void test_create_uvm_find_pte() {
    uint64_t total_nums = get_available_page_count(&g_alloc);
    printf("test start total nums: %d\n", total_nums);

    
    // 测试create_uvm函数
    pte_t *page_dir = create_uvm();
    assert(page_dir != (pte_t*)0);
    printf("Page directory created at: %x\n", page_dir);

    // 测试find_pte是否能够正确分配并返回页表项
    uint64_t vaddr = 0x54567890;  // 假设这是一个虚拟地址
    /*                            162       359   
        000000000 000000001 010100010 101100111 1000 1001 0000
     */
    pte_t *pte = find_pte(page_dir, vaddr, 1);
    assert(pte != NULL);
    printf("Page table entry for vaddr 0x%lx: 0x%x\n", vaddr, pte);

    pte_t *test = find_pte(page_dir, vaddr, 1);
    assert(pte == test);

    destory_4level(page_dir);

    
    // 保证测试完成时总页数相同
    uint64_t end_total_nums = get_available_page_count(&g_alloc);
    printf("test end total nums: %d\n", end_total_nums);
    assert(total_nums == end_total_nums);
}

// 测试 memory_create_map 函数 memory_get_paddr 函数 和 memory_free_page 函数
void test_memory_create_map() {
    uint64_t total_nums = get_available_page_count(&g_alloc);
    printf("test start total nums: %d\n", total_nums);

    pte_t *page_dir = create_uvm();
    assert(page_dir != (pte_t*)0);
    printf("Page directory created at: %lx\n", (unsigned long)page_dir);

    uint64_t vaddr = 0x1000;                        // 虚拟地址
    uint64_t paddr = addr_alloc_page(&g_alloc, 3);  // 物理地址
    int count = 3;  // 映射2个页面

    int result = memory_create_map(page_dir, vaddr, paddr, count, 0x0);  // 假设权限为0x0
    assert (result == 0);

    // 测试映射结果：验证虚拟地址映射到的物理地址
    uint64_t mapped_paddr = memory_get_paddr(page_dir, vaddr);
    assert (mapped_paddr == paddr);

    // 验证第二个虚拟地址
    uint64_t vaddr2 = vaddr + PAGE_SIZE;  // 第二个页面
    uint64_t mapped_paddr2 = memory_get_paddr(page_dir, vaddr2);
    assert (mapped_paddr2 == paddr + PAGE_SIZE) ;

    uint64_t vaddr3 = vaddr + PAGE_SIZE * 2;  // 第二个页面
    uint64_t mapped_paddr3 = memory_get_paddr(page_dir, vaddr3);
    assert (mapped_paddr3 == paddr + PAGE_SIZE * 2) ;

    printf("test free page: \n");
    memory_free_page(page_dir, vaddr);
    memory_free_page(page_dir, vaddr2);
    memory_free_page(page_dir, vaddr3);

    destory_4level(page_dir);

        
    // 保证测试完成时总页数相同
    uint64_t end_total_nums = get_available_page_count(&g_alloc);
    printf("test end total nums: %d\n", end_total_nums);
    assert(total_nums == end_total_nums);
}

// 测试 memory_alloc_page memory_free_page
void test_uvm_alloc_free() {
    uint64_t total_nums = get_available_page_count(&g_alloc);
    printf("test start total nums: %d\n", total_nums);

    pte_t *page_dir = create_uvm();
    assert(page_dir != (pte_t*)0);
    printf("Page directory created at: %lx\n", (unsigned long)page_dir);


    memory_alloc_page(page_dir, 0x1000, 34, 0);
    memory_free_page(page_dir, 0x1000);

    destory_4level(page_dir);

    // 保证测试完成时总页数相同
    uint64_t end_total_nums = get_available_page_count(&g_alloc);
    printf("test end total nums: %d\n", end_total_nums);
    assert(total_nums == end_total_nums);
}

// 测试 内核将数据拷贝到指定进程空间下
void test_copydata_to_uvm() {
    uint64_t total_nums = get_available_page_count(&g_alloc);
    printf("test start total nums: %d\n", total_nums);

    pte_t *page_dir = create_uvm();
    assert(page_dir != (pte_t*)0);
    printf("Page directory created at: %lx\n", (unsigned long)page_dir);

    // 准备内核数据
    char data[156] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    uint64_t paddr = addr_alloc_page(&g_alloc, 1);
    memcpy((const void *)paddr, data, 156);

    // 为进程空间申请内存
    memory_alloc_page(page_dir, 0x1000, 156, 0);
    copydata_to_uvm(page_dir, 0x1000, paddr, 156);

    // 这里可以查看数据是否正确
    uint64_t paddr_to_check = memory_get_paddr(page_dir, 0x1000);
    if(memcmp((const void *)paddr, (const void *)paddr_to_check, 156) == 0) {
        printf("data ok\n");
    }

    // 清理进程空间和内核内存
    memory_free_page(page_dir, 0x1000);
    addr_free_page(&g_alloc, paddr, 1);

    destory_4level(page_dir);

    // 保证测试完成时总页数相同
    uint64_t end_total_nums = get_available_page_count(&g_alloc);
    printf("test end total nums: %d\n", end_total_nums);
    assert(total_nums == end_total_nums);
}


uint64_t mock_addr_alloc_page(void *alloc, int count) {
    // Simulate out-of-memory failure
    return (count == 1) ? 0 : addr_alloc_page(alloc, count);  // Fail for single-page allocation
}

bool validate_memory_content(uint64_t src_addr, uint64_t dst_addr, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (*(uint8_t *)(src_addr + i) != *(uint8_t *)(dst_addr + i)) {
            return false;
        }
    }
    return true;
}

void validate_page_table(pte_t *page_dir, uint64_t vaddr, uint64_t paddr) {
    uint64_t fetched_paddr = memory_get_paddr(page_dir, vaddr);
    assert(fetched_paddr == paddr);
}

void test_memory_copy_uvm_4level() {
    uint64_t total_nums = get_available_page_count(&g_alloc);
    printf("test start total nums: %d\n", total_nums);

    // Create source and destination page tables
    pte_t *src_pgd = create_uvm();
    pte_t *dst_pgd = create_uvm();

    // 准备内核数据
    uint64_t src_phys = addr_alloc_page(&g_alloc, 1);
    memcpy((void *)src_phys, "test data", 9);  // Copy some data into the page
    
        // 为进程分配内存
        memory_alloc_page(src_pgd, 0x1000, PAGE_SIZE, 0);  // Allocate a page
        // 内核将数据拷贝到指定进程空间下
        copydata_to_uvm(src_pgd, 0x1000, src_phys, 9);

        int result = memory_copy_uvm_4level(dst_pgd, src_pgd);
        assert(result == 0);

        // Validate the data in the destination page table
        uint64_t dst_phys = memory_get_paddr(dst_pgd, 0x1000);
        assert(validate_memory_content(src_phys, dst_phys, PAGE_SIZE));

        // Clean up
        memory_free_page(src_pgd, 0x1000);
        memory_free_page(dst_pgd, 0x1000);

        destroy_uvm_4level(src_pgd);
        destroy_uvm_4level(dst_pgd);
    
    addr_free_page(&g_alloc, src_phys, 1);

    // 保证测试完成时总页数相同
    uint64_t end_total_nums = get_available_page_count(&g_alloc);
    printf("test end total nums: %d\n", end_total_nums);
    assert(total_nums == end_total_nums);
}

void kmem_test() 
{
    /*
     * 这里每个函数测试完成都会保证页释放。
     */
    // uint64_t total_nums = get_available_page_count(&g_alloc);
    // printf("test start total nums: %d\n", total_nums);
    // test_alloc_free();
    // test_find_free_page();
    // test_find_contiguous_free_pages();
    // uint64_t end_total_nums = get_available_page_count(&g_alloc);
    // printf("test end total nums: %d\n", end_total_nums);
    // assert(total_nums == end_total_nums);

    // printf("\n\n========== uvm tests: =========\n\n");
    // test_create_uvm_find_pte();

    // printf("\n\n=========map and find paddr tests: =========\n\n");
    // test_memory_create_map();

    // printf("\n\n=========uvm alloc free tests: =========\n\n");
    // test_uvm_alloc_free();

    // test_copydata_to_uvm();

    // test_memory_copy_uvm_4level();
}
