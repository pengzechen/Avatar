
#include <aj_types.h>
#include <mem/mmu.h>
#include <mem/aj_string.h>
#include <mem/page.h>
#include <os_cfg.h>
#include <mem/barrier.h>

// 定义页表
static uint64_t pt0[512] __attribute__((aligned(4096)));
static uint64_t pt1[512] __attribute__((aligned(4096)));

static uint64_t high_pt0[512] __attribute__((aligned(4096)));
static uint64_t high_pt1[512] __attribute__((aligned(4096)));

static bool mmu_enable_flag;

size_t cacheline_bytes;

// 定义一个函数来读取 CWG 字段的值
uint32_t get_cache_line_size()
{

    uint64_t ctr_el0;
    uint32_t cwg;

    // 读取 CTR_EL0 寄存器到 ctr_el0 变量
    __asm__ __volatile__("mrs %0, ctr_el0" : "=r"(ctr_el0));

    // 提取 CWG 字段的值
    cwg = (ctr_el0 >> 0) & CTR_EL0_CWG_MASK;

    return cwg;
}

void setup_cache(void)
{
    cacheline_bytes = get_cache_line_size();
}

typedef struct _table_t{
    uint64_t is_valid : 1, 
    is_table : 1, 
    ignored1 : 10,
    next_table_addr : 36, 
    reserved : 4, 
    ignored2 : 7,
    PXNTable : 1, // Privileged Execute-never for next level
    XNTable : 1,  // Execute-never for next level
    APTable : 2,  // Access permissions for next level
    NSTable : 1;
} table_t;

// 使用 9 9 9 9 12 48位的虚拟地址 直接映射4个G
void init_page_table()
{
    setup_cache();
    memset((void *)pt0, 0, sizeof(uint64_t) * 512);
    memset((void *)pt1, 0, sizeof(uint64_t) * 512);
    memset((void *)high_pt0, 0, sizeof(uint64_t) * 512);
    memset((void *)high_pt1, 0, sizeof(uint64_t) * 512);
    
    pt0[0] = (uint64_t)pt1 + 0b11;

    pt1[0] = (0x00000000ULL | PTE_DEVICE_MEMORY ); // 0x0000_0000 0x4000_0000
    pt1[1] = (0x40000000ULL | PTE_NORMAL_MEMORY ); // 0x4000_0000 0x8000_0000
    
    pt1[2] = (0x80000000ULL | PTE_NORMAL_MEMORY | _AP_EL0 | _PXN); // 0x8000_0000 0xc000_0000         // 这个G分给el0使用
    pt1[3] = (0xc0000000ULL | PTE_NORMAL_MEMORY | _AP_EL0 | _PXN); // 0xc000_0000 0x1000_0000_0000    // 这个G分给el0使用

    high_pt0[0] = (uint64_t)high_pt1 + 0b11;

    high_pt1[0] = (0x00000000ULL | PTE_DEVICE_MEMORY ); // 0x0000_0000 0x4000_0000
    high_pt1[1] = (0x40000000ULL | PTE_NORMAL_MEMORY ); // 0x4000_0000 0x8000_0000
}

extern void init_mmu(uint64_t, uint64_t);
extern void init_mmu_el2(uint64_t);

void enable_mmu()
{
    init_mmu((uint64_t)(void *)pt0, (uint64_t)(void *)high_pt0);
}

void enable_mmu_el2()
{
    init_mmu_el2((uint64_t)(void *)pt0);
}