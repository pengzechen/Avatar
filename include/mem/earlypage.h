/**
 * @file earlypage.h
 * @brief 启动时页表管理接口和配置
 */

#ifndef __EARLYPAGE_H__
#define __EARLYPAGE_H__

#include "avatar_types.h"

/* 页表配置常量 */
#define PAGE_TABLE_ENTRIES          512         // 每个页表的条目数
#define PAGE_TABLE_ALIGNMENT        4096        // 页表对齐要求 (4KB)
#define MEMORY_BLOCK_1GB            0x40000000ULL  // 1GB内存块大小

/* 内存区域基址定义 - 可根据硬件平台调整 */
#define DEVICE_MEMORY_BASE          0x00000000ULL  // 设备内存起始地址
#define NORMAL_MEMORY_BASE          0x40000000ULL  // 普通内存起始地址  
#define USER_MEMORY_BASE            0x80000000ULL  // 用户内存起始地址
#define USER_MEMORY_EXT_BASE        0xc0000000ULL  // 扩展用户内存起始地址

/* CTR_EL0寄存器字段定义 */
#define CTR_EL0_DMINLINE_SHIFT      16          // DminLine字段位移
#define CTR_EL0_DMINLINE_MASK       0xF         // DminLine字段掩码
#define CACHE_LINE_WORD_SIZE        4           // 缓存行字大小

/* 缓存配置常量 */
#define DEFAULT_CACHELINE_SIZE      64          // 默认缓存行大小(字节)
#define MIN_CACHELINE_SIZE          16          // 最小缓存行大小(字节)
#define MAX_CACHELINE_SIZE          256         // 最大缓存行大小(字节)

/* 页表项标志 */
#define PTE_TABLE_FLAGS             0b11        // 页表项标志 (有效+表)

/* 内存区域大小计算宏 */
#define DEVICE_MEMORY_SIZE          (NORMAL_MEMORY_BASE - DEVICE_MEMORY_BASE)
#define NORMAL_MEMORY_SIZE          (USER_MEMORY_BASE - NORMAL_MEMORY_BASE)
#define USER_MEMORY_SIZE            (USER_MEMORY_EXT_BASE - USER_MEMORY_BASE)

/* 配置验证宏 */
#define IS_1GB_ALIGNED(addr)        (((addr) & (MEMORY_BLOCK_1GB - 1)) == 0)
#define IS_VALID_MEMORY_LAYOUT()    (DEVICE_MEMORY_BASE < NORMAL_MEMORY_BASE && \
                                     NORMAL_MEMORY_BASE < USER_MEMORY_BASE && \
                                     USER_MEMORY_BASE < USER_MEMORY_EXT_BASE)

/* ==================== 函数接口声明 ==================== */

/* 页表初始化和MMU控制 */
void init_page_table(void);
void enable_mmu(void);
void enable_mmu_el2(void);
uint64_t get_kpgdir(void);

/* 状态查询函数 */
bool is_mmu_enabled(void);
size_t get_cacheline_size(void);

#endif // __EARLYPAGE_H__
