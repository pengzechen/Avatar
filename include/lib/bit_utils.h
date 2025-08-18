/**
 * @file bit_utils.h
 * @brief Avatar OS 位操作工具函数和宏
 * 
 * 提供常用的位操作工具函数和宏定义，用于简化位操作相关的代码
 */

#ifndef __BIT_UTILS_H__
#define __BIT_UTILS_H__

#include "../avatar_types.h"

/* ==================== 位操作宏定义 ==================== */

/**
 * @brief 获取数值的最高位位置
 * @param x 要检查的数值
 * @return 最高位的位置（从0开始计数）
 * 
 * 例如：
 * - HIGHEST_BIT_POSITION(0) = 0
 * - HIGHEST_BIT_POSITION(1) = 0  
 * - HIGHEST_BIT_POSITION(2) = 1
 * - HIGHEST_BIT_POSITION(4) = 2
 * - HIGHEST_BIT_POSITION(8) = 3
 */
#define HIGHEST_BIT_POSITION(x)                                                                    \
    ({                                                                                             \
        uint32_t           _i   = 0;                                                               \
        unsigned long long _val = (x);                                                             \
        while (_val >>= 1) {                                                                       \
            _i++;                                                                                  \
        }                                                                                          \
        _i;                                                                                        \
    })

/**
 * @brief 设置指定位
 * @param value 原始值
 * @param bit 要设置的位位置（从0开始）
 * @return 设置指定位后的值
 */
#define BIT_SET(value, bit) ((value) | (1ULL << (bit)))

/**
 * @brief 清除指定位
 * @param value 原始值
 * @param bit 要清除的位位置（从0开始）
 * @return 清除指定位后的值
 */
#define BIT_CLEAR(value, bit) ((value) & ~(1ULL << (bit)))

/**
 * @brief 切换指定位
 * @param value 原始值
 * @param bit 要切换的位位置（从0开始）
 * @return 切换指定位后的值
 */
#define BIT_TOGGLE(value, bit) ((value) ^ (1ULL << (bit)))

/**
 * @brief 检查指定位是否设置
 * @param value 要检查的值
 * @param bit 要检查的位位置（从0开始）
 * @return 如果位被设置返回非零值，否则返回0
 */
#define BIT_TEST(value, bit) ((value) & (1ULL << (bit)))

/**
 * @brief 获取指定位的值（0或1）
 * @param value 要检查的值
 * @param bit 要获取的位位置（从0开始）
 * @return 位的值（0或1）
 */
#define BIT_GET(value, bit) (((value) >> (bit)) & 1)

/**
 * @brief 创建位掩码
 * @param start 起始位位置（包含）
 * @param end 结束位位置（包含）
 * @return 位掩码
 */
#define BIT_MASK(start, end) (((1ULL << ((end) - (start) + 1)) - 1) << (start))

/**
 * @brief 提取指定位范围的值
 * @param value 原始值
 * @param start 起始位位置（包含）
 * @param end 结束位位置（包含）
 * @return 提取的位范围值
 */
#define BIT_EXTRACT(value, start, end) (((value) & BIT_MASK(start, end)) >> (start))

/**
 * @brief 在指定位范围插入值
 * @param original 原始值
 * @param value 要插入的值
 * @param start 起始位位置（包含）
 * @param end 结束位位置（包含）
 * @return 插入值后的结果
 */
#define BIT_INSERT(original, value, start, end)                                                    \
    (((original) & ~BIT_MASK(start, end)) | (((value) << (start)) & BIT_MASK(start, end)))

/* ==================== 位计数宏定义 ==================== */

/**
 * @brief 计算32位数值中设置的位数（使用GCC内建函数）
 * @param x 要计算的32位数值
 * @return 设置的位数
 */
#define BIT_COUNT_32(x) __builtin_popcount(x)

/**
 * @brief 计算64位数值中设置的位数（使用GCC内建函数）
 * @param x 要计算的64位数值
 * @return 设置的位数
 */
#define BIT_COUNT_64(x) __builtin_popcountll(x)

/**
 * @brief 查找32位数值中第一个设置的位（从低位开始，使用GCC内建函数）
 * @param x 要查找的32位数值
 * @return 第一个设置位的位置（从1开始），如果没有设置的位返回0
 */
#define BIT_FIND_FIRST_SET_32(x) __builtin_ffs(x)

/**
 * @brief 查找64位数值中第一个设置的位（从低位开始，使用GCC内建函数）
 * @param x 要查找的64位数值
 * @return 第一个设置位的位置（从1开始），如果没有设置的位返回0
 */
#define BIT_FIND_FIRST_SET_64(x) __builtin_ffsll(x)

/**
 * @brief 计算32位数值前导零的个数（使用GCC内建函数）
 * @param x 要计算的32位数值（不能为0）
 * @return 前导零的个数
 */
#define BIT_COUNT_LEADING_ZEROS_32(x) __builtin_clz(x)

/**
 * @brief 计算64位数值前导零的个数（使用GCC内建函数）
 * @param x 要计算的64位数值（不能为0）
 * @return 前导零的个数
 */
#define BIT_COUNT_LEADING_ZEROS_64(x) __builtin_clzll(x)

/**
 * @brief 计算32位数值尾随零的个数（使用GCC内建函数）
 * @param x 要计算的32位数值（不能为0）
 * @return 尾随零的个数
 */
#define BIT_COUNT_TRAILING_ZEROS_32(x) __builtin_ctz(x)

/**
 * @brief 计算64位数值尾随零的个数（使用GCC内建函数）
 * @param x 要计算的64位数值（不能为0）
 * @return 尾随零的个数
 */
#define BIT_COUNT_TRAILING_ZEROS_64(x) __builtin_ctzll(x)

/* ==================== 对齐相关宏定义 ==================== */

/**
 * @brief 检查数值是否是2的幂
 * @param x 要检查的数值
 * @return 如果是2的幂返回非零值，否则返回0
 */
#define IS_POWER_OF_2(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)

/**
 * @brief 向上对齐到2的幂
 * @param x 要对齐的数值
 * @param align 对齐值（必须是2的幂）
 * @return 对齐后的值
 */
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

/**
 * @brief 向下对齐到2的幂
 * @param x 要对齐的数值
 * @param align 对齐值（必须是2的幂）
 * @return 对齐后的值
 */
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

/**
 * @brief 检查数值是否已对齐
 * @param x 要检查的数值
 * @param align 对齐值（必须是2的幂）
 * @return 如果已对齐返回非零值，否则返回0
 */
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

/* ==================== 字节序转换宏定义 ==================== */

/**
 * @brief 16位字节序转换
 * @param x 要转换的16位值
 * @return 字节序转换后的值
 */
#define BSWAP16(x) __builtin_bswap16(x)

/**
 * @brief 32位字节序转换
 * @param x 要转换的32位值
 * @return 字节序转换后的值
 */
#define BSWAP32(x) __builtin_bswap32(x)

/**
 * @brief 64位字节序转换
 * @param x 要转换的64位值
 * @return 字节序转换后的值
 */
#define BSWAP64(x) __builtin_bswap64(x)

#endif  // __BIT_UTILS_H__
