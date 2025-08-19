/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file fat32_utils.h
 * @brief Implementation of fat32_utils.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file fat32_utils.h
 * @brief FAT32工具函数头文件
 * 
 * 本文件定义了FAT32文件系统的各种工具函数，包括：
 * - 字符串处理和路径解析
 * - 时间和日期转换
 * - 数据格式转换
 * - 校验和计算
 * - 调试辅助函数
 */

#ifndef FAT32_UTILS_H
#define FAT32_UTILS_H

#include "fat32_types.h"
#include "avatar_types.h"

/* ============================================================================
 * 字符串和路径处理函数
 * ============================================================================ */

/**
 * @brief 规范化路径
 * 
 * 将路径转换为标准格式，处理"."和".."等特殊目录。
 * 
 * @param path 输入路径
 * @param normalized_path 输出的规范化路径
 * @param max_len 输出缓冲区最大长度
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_utils_normalize_path(const char *path, char *normalized_path, size_t max_len);

/**
 * @brief 分割路径
 * 
 * 将完整路径分割为目录部分和文件名部分。
 * 
 * @param full_path 完整路径
 * @param dir_path 输出的目录路径
 * @param filename 输出的文件名
 * @param max_dir_len 目录路径缓冲区最大长度
 * @param max_name_len 文件名缓冲区最大长度
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_utils_split_path(const char *full_path,
                       char       *dir_path,
                       char       *filename,
                       size_t      max_dir_len,
                       size_t      max_name_len);

/**
 * @brief 连接路径
 * 
 * 将目录路径和文件名连接成完整路径。
 * 
 * @param dir_path 目录路径
 * @param filename 文件名
 * @param full_path 输出的完整路径
 * @param max_len 输出缓冲区最大长度
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_utils_join_path(const char *dir_path, const char *filename, char *full_path, size_t max_len);

/**
 * @brief 检查路径是否为绝对路径
 * 
 * @param path 路径字符串
 * @return uint8_t 1表示绝对路径，0表示相对路径
 */
uint8_t
fat32_utils_is_absolute_path(const char *path);

/**
 * @brief 获取文件扩展名
 * 
 * @param filename 文件名
 * @param extension 输出的扩展名（不包含点号）
 * @param max_len 输出缓冲区最大长度
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_utils_get_file_extension(const char *filename, char *extension, size_t max_len);

/* ============================================================================
 * 时间和日期转换函数
 * ============================================================================ */

/**
 * @brief 简化的时间结构
 */
typedef struct
{
    uint16_t year;    // 年份（1980-2107）
    uint8_t  month;   // 月份（1-12）
    uint8_t  day;     // 日期（1-31）
    uint8_t  hour;    // 小时（0-23）
    uint8_t  minute;  // 分钟（0-59）
    uint8_t  second;  // 秒（0-59，2秒精度）
} fat32_datetime_t;

/**
 * @brief 将FAT32时间格式转换为时间结构
 * 
 * @param fat_time FAT32时间字段
 * @param fat_date FAT32日期字段
 * @param datetime 输出的时间结构
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_utils_fat_to_datetime(uint16_t fat_time, uint16_t fat_date, fat32_datetime_t *datetime);

/**
 * @brief 将时间结构转换为FAT32时间格式
 * 
 * @param datetime 时间结构
 * @param fat_time 输出的FAT32时间字段
 * @param fat_date 输出的FAT32日期字段
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_utils_datetime_to_fat(const fat32_datetime_t *datetime,
                            uint16_t               *fat_time,
                            uint16_t               *fat_date);

/**
 * @brief 获取当前时间（简化实现）
 * 
 * @param datetime 输出的当前时间
 * @return fat32_error_t 错误码
 * 
 * 简化说明：返回固定的时间值，实际实现中应该获取系统时间
 */
fat32_error_t
fat32_utils_get_current_time(fat32_datetime_t *datetime);

/* ============================================================================
 * 数据格式转换函数
 * ============================================================================ */

/**
 * @brief 将小端序16位整数转换为主机字节序
 * 
 * @param le_value 小端序值
 * @return uint16_t 主机字节序值
 */
uint16_t
fat32_utils_le16_to_host(uint16_t le_value);

/**
 * @brief 将主机字节序16位整数转换为小端序
 * 
 * @param host_value 主机字节序值
 * @return uint16_t 小端序值
 */
uint16_t
fat32_utils_host_to_le16(uint16_t host_value);

/**
 * @brief 将小端序32位整数转换为主机字节序
 * 
 * @param le_value 小端序值
 * @return uint32_t 主机字节序值
 */
uint32_t
fat32_utils_le32_to_host(uint32_t le_value);

/**
 * @brief 将主机字节序32位整数转换为小端序
 * 
 * @param host_value 主机字节序值
 * @return uint32_t 小端序值
 */
uint32_t
fat32_utils_host_to_le32(uint32_t host_value);

/* ============================================================================
 * 校验和计算函数
 * ============================================================================ */

/**
 * @brief 计算短文件名的校验和
 * 
 * 用于长文件名目录项的校验和字段。
 * 
 * @param short_name 8.3格式的短文件名（11字节）
 * @return uint8_t 校验和值
 */
uint8_t
fat32_utils_calculate_checksum(const uint8_t *short_name);

/**
 * @brief 验证短文件名的校验和
 * 
 * @param short_name 8.3格式的短文件名（11字节）
 * @param checksum 要验证的校验和
 * @return uint8_t 1表示校验和正确，0表示错误
 */
uint8_t
fat32_utils_verify_checksum(const uint8_t *short_name, uint8_t checksum);

/* ============================================================================
 * 调试辅助函数
 * ============================================================================ */

/**
 * @brief 打印十六进制数据
 * 
 * 以十六进制格式打印数据，用于调试。
 * 
 * @param data 数据指针
 * @param size 数据大小
 * @param prefix 打印前缀
 */
void
fat32_utils_print_hex(const void *data, size_t size, const char *prefix);

/**
 * @brief 打印目录项信息
 * 
 * 以可读格式打印目录项的详细信息。
 * 
 * @param dir_entry 目录项指针
 */
void
fat32_utils_print_dir_entry(const fat32_dir_entry_t *dir_entry);

/**
 * @brief 打印文件属性
 * 
 * 以可读格式打印文件属性标志。
 * 
 * @param attr 属性字节
 */
void
fat32_utils_print_file_attributes(uint8_t attr);

/**
 * @brief 格式化文件大小
 * 
 * 将文件大小格式化为可读的字符串（如"1.5 KB"）。
 * 
 * @param size 文件大小（字节）
 * @param formatted_size 输出的格式化字符串
 * @param max_len 输出缓冲区最大长度
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_utils_format_file_size(uint32_t size, char *formatted_size, size_t max_len);

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */

/**
 * @brief 检查字符是否为路径分隔符
 * 
 * @param c 字符
 * @return uint8_t 1表示是路径分隔符，0表示不是
 */
static inline uint8_t
fat32_utils_is_path_separator(char c)
{
    return (c == '/' || c == '\\');
}

/**
 * @brief 将字符转换为大写
 * 
 * @param c 输入字符
 * @return char 大写字符
 */
static inline char
fat32_utils_to_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
}

/**
 * @brief 将字符转换为小写
 * 
 * @param c 输入字符
 * @return char 小写字符
 */
static inline char
fat32_utils_to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
}

/**
 * @brief 检查字符是否为字母数字
 * 
 * @param c 字符
 * @return uint8_t 1表示是字母数字，0表示不是
 */
static inline uint8_t
fat32_utils_is_alnum(char c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
}

/**
 * @brief 对齐到指定边界
 * 
 * @param value 要对齐的值
 * @param alignment 对齐边界
 * @return uint32_t 对齐后的值
 */
static inline uint32_t
fat32_utils_align_up(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief 向下对齐到指定边界
 * 
 * @param value 要对齐的值
 * @param alignment 对齐边界
 * @return uint32_t 对齐后的值
 */
static inline uint32_t
fat32_utils_align_down(uint32_t value, uint32_t alignment)
{
    return value & ~(alignment - 1);
}

#endif  // FAT32_UTILS_H
