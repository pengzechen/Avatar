
/**
 * @file earlypage.c
 * @brief 启动时页表管理 - 简单的1GB块映射
 */

#include "avatar_types.h"
#include "mem/mmu.h"
#include "lib/avatar_string.h"
#include "mem/earlypage.h"
#include "os_cfg.h"
#include "mem/barrier.h"

/* 页表定义 - 启动时使用的简单映射 */
static uint64_t pt0[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_TABLE_ALIGNMENT)));      // 低地址空间L0页表
static uint64_t pt1[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_TABLE_ALIGNMENT)));      // 低地址空间L1页表

static uint64_t high_pt0[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_TABLE_ALIGNMENT))); // 高地址空间L0页表
static uint64_t high_pt1[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_TABLE_ALIGNMENT))); // 高地址空间L1页表

static bool mmu_enable_flag = false;
static size_t cacheline_bytes = 0;  // 缓存行大小，0表示未初始化

/* 外部汇编函数声明 */
extern void init_mmu(uint64_t ttbr0, uint64_t ttbr1);
extern void init_mmu_el2(uint64_t ttbr0);

/**
 * @brief 验证内存布局配置的一致性
 * @return true表示配置有效，false表示配置错误
 */
static bool validate_memory_layout(void)
{
    // 验证内存区域布局的合理性
    if (!IS_VALID_MEMORY_LAYOUT()) {
        return false;
    }

    // 验证每个区域都是1GB对齐的
    if (!IS_1GB_ALIGNED(DEVICE_MEMORY_BASE) ||
        !IS_1GB_ALIGNED(NORMAL_MEMORY_BASE) ||
        !IS_1GB_ALIGNED(USER_MEMORY_BASE) ||
        !IS_1GB_ALIGNED(USER_MEMORY_EXT_BASE)) {
        return false;
    }

    return true;
}

/**
 * @brief 设置页表项指向下一级页表
 * @param pte 页表项指针
 * @param next_table 下一级页表地址
 */
static inline void set_table_entry(uint64_t *pte, uint64_t *next_table)
{
    *pte = (uint64_t)next_table | PTE_TABLE_FLAGS;
}

/**
 * @brief 设置1GB块映射页表项
 * @param pte 页表项指针
 * @param base_addr 物理基址
 * @param memory_type 内存类型标志
 * @param access_flags 访问权限标志
 */
static inline void set_block_entry(uint64_t *pte, uint64_t base_addr,
                                   uint64_t memory_type, uint64_t access_flags)
{
    *pte = base_addr | memory_type | access_flags;
}

/**
 * @brief 从硬件寄存器读取CPU缓存行大小
 * @return 缓存行大小(字节)，失败返回0
 */
static size_t read_cpu_cacheline_size(void)
{
    uint64_t ctr_el0;
    uint32_t dminline;

    __asm__ __volatile__("mrs %0, ctr_el0" : "=r"(ctr_el0));

    // 提取DminLine字段 (bits [19:16])
    dminline = (ctr_el0 >> CTR_EL0_DMINLINE_SHIFT) & CTR_EL0_DMINLINE_MASK;

    // 计算缓存行大小: WORD_SIZE * 2^DminLine
    size_t cache_size = CACHE_LINE_WORD_SIZE << dminline;

    // 验证缓存行大小的合理性
    if (cache_size < MIN_CACHELINE_SIZE || cache_size > MAX_CACHELINE_SIZE) {
        return 0;  // 无效的缓存行大小
    }

    return cache_size;
}

/**
 * @brief 初始化缓存配置
 * @return true表示成功，false表示失败
 */
static bool init_cache_config(void)
{
    if (cacheline_bytes != 0) {
        return true;  // 已经初始化过了
    }

    cacheline_bytes = read_cpu_cacheline_size();
    return (cacheline_bytes != 0);
}

/**
 * @brief 初始化页表 - 使用48位虚拟地址直接映射4GB物理内存
 *
 * 内存布局:
 * - DEVICE_MEMORY_BASE-NORMAL_MEMORY_BASE: 设备内存 (1GB)
 * - NORMAL_MEMORY_BASE-USER_MEMORY_BASE: 普通内存 (1GB)
 * - USER_MEMORY_BASE-USER_MEMORY_EXT_BASE: EL0用户内存 (1GB)
 * - USER_MEMORY_EXT_BASE以上: EL0用户内存 (1GB)
 */
void init_page_table(void)
{
    // 验证内存布局配置
    if (!validate_memory_layout()) {
        // 配置错误，无法继续初始化
        return;
    }

    // 初始化缓存配置
    if (!init_cache_config()) {
        // 缓存配置失败，使用默认值
        cacheline_bytes = DEFAULT_CACHELINE_SIZE;
    }

    // 清零所有页表
    memset((void *)pt0, 0, sizeof(pt0));
    memset((void *)pt1, 0, sizeof(pt1));
    memset((void *)high_pt0, 0, sizeof(high_pt0));
    memset((void *)high_pt1, 0, sizeof(high_pt1));

    // 设置低地址空间页表 (TTBR0_EL1)
    set_table_entry(&pt0[0], pt1);  // L0[0] -> L1页表

    // L1页表映射 - 每项映射1GB
    set_block_entry(&pt1[0], DEVICE_MEMORY_BASE, PTE_DEVICE_MEMORY, 0);                    // 设备内存区域
    set_block_entry(&pt1[1], NORMAL_MEMORY_BASE, PTE_NORMAL_MEMORY, 0);                    // 内核普通内存
    set_block_entry(&pt1[2], USER_MEMORY_BASE, PTE_NORMAL_MEMORY, _AP_EL0 | _PXN);         // EL0用户内存 (禁止特权执行)
    set_block_entry(&pt1[3], USER_MEMORY_EXT_BASE, PTE_NORMAL_MEMORY, _AP_EL0 | _PXN);     // EL0用户内存 (禁止特权执行)

    // 设置高地址空间页表 (TTBR1_EL1) - 内核空间
    set_table_entry(&high_pt0[0], high_pt1);  // L0[0] -> L1页表

    // 高地址空间L1页表映射
    set_block_entry(&high_pt1[0], DEVICE_MEMORY_BASE, PTE_DEVICE_MEMORY, 0);  // 设备内存区域
    set_block_entry(&high_pt1[1], NORMAL_MEMORY_BASE, PTE_NORMAL_MEMORY, 0);  // 内核普通内存
}

/**
 * @brief 启用EL1的MMU
 */
void enable_mmu(void)
{
    init_mmu((uint64_t)(void *)pt0, (uint64_t)(void *)high_pt0);
    mmu_enable_flag = true;
}

/**
 * @brief 启用EL2的MMU
 */
void enable_mmu_el2(void)
{
    init_mmu_el2((uint64_t)(void *)pt0);
    mmu_enable_flag = true;
}

/**
 * @brief 获取内核页目录基址
 * @return 页表物理地址
 */
uint64_t get_kpgdir(void)
{
    return (uint64_t)(void *)pt0;
}

/**
 * @brief 检查MMU是否已启用
 * @return true表示已启用，false表示未启用
 */
bool is_mmu_enabled(void)
{
    return mmu_enable_flag;
}

/**
 * @brief 获取缓存行大小
 * @return 缓存行大小(字节)，如果未初始化则返回默认值
 */
size_t get_cacheline_size(void)
{
    if (cacheline_bytes == 0) {
        // 如果还未初始化，尝试初始化
        if (!init_cache_config()) {
            return DEFAULT_CACHELINE_SIZE;  // 返回默认值
        }
    }
    return cacheline_bytes;
}

