/**
 * @file fat32_utils.c
 * @brief FAT32工具函数实现
 * 
 * 本文件实现了FAT32文件系统的各种工具函数。
 */

#include "fs/fat32_utils.h"
#include "fs/fat32_dir.h"
#include "lib/avatar_string.h"
#include "lib/avatar_assert.h"
#include "io.h"

/* ============================================================================
 * 字符串和路径处理函数实现
 * ============================================================================ */

fat32_error_t fat32_utils_normalize_path(const char *path, 
                                         char *normalized_path, 
                                         size_t max_len)
{
    avatar_assert(path != NULL);
    avatar_assert(normalized_path != NULL);
    
    size_t path_len = strlen(path);
    if (path_len == 0 || path_len >= max_len) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 简化实现：只处理基本的路径规范化
    // 将反斜杠转换为正斜杠，移除重复的斜杠
    
    size_t out_pos = 0;
    uint8_t last_was_slash = 0;
    
    for (size_t i = 0; i < path_len && out_pos < max_len - 1; i++) {
        char c = path[i];
        
        if (fat32_utils_is_path_separator(c)) {
            if (!last_was_slash) {
                normalized_path[out_pos++] = '/';
                last_was_slash = 1;
            }
        } else {
            normalized_path[out_pos++] = c;
            last_was_slash = 0;
        }
    }
    
    // 确保路径以斜杠开头（绝对路径）
    if (out_pos == 0 || normalized_path[0] != '/') {
        if (out_pos >= max_len - 1) {
            return FAT32_ERROR_INVALID_PARAM;
        }
        memmove(normalized_path + 1, normalized_path, out_pos);
        normalized_path[0] = '/';
        out_pos++;
    }
    
    // 移除末尾的斜杠（除非是根目录）
    if (out_pos > 1 && normalized_path[out_pos - 1] == '/') {
        out_pos--;
    }
    
    normalized_path[out_pos] = '\0';
    return FAT32_OK;
}

fat32_error_t fat32_utils_split_path(const char *full_path,
                                     char *dir_path,
                                     char *filename,
                                     size_t max_dir_len,
                                     size_t max_name_len)
{
    avatar_assert(full_path != NULL);
    avatar_assert(dir_path != NULL);
    avatar_assert(filename != NULL);
    
    size_t path_len = strlen(full_path);
    if (path_len == 0) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 查找最后一个路径分隔符
    const char *last_slash = NULL;
    for (int i = path_len - 1; i >= 0; i--) {
        if (fat32_utils_is_path_separator(full_path[i])) {
            last_slash = &full_path[i];
            break;
        }
    }
    
    if (last_slash == NULL) {
        // 没有路径分隔符，整个路径都是文件名
        if (strlen(full_path) >= max_name_len) {
            return FAT32_ERROR_INVALID_PARAM;
        }
        strcpy(dir_path, "/");
        strcpy(filename, full_path);
    } else {
        // 分离目录和文件名
        size_t dir_len = last_slash - full_path;
        if (dir_len == 0) {
            dir_len = 1;  // 根目录
        }
        
        if (dir_len >= max_dir_len || strlen(last_slash + 1) >= max_name_len) {
            return FAT32_ERROR_INVALID_PARAM;
        }
        
        strncpy(dir_path, full_path, dir_len);
        dir_path[dir_len] = '\0';
        strcpy(filename, last_slash + 1);
    }
    
    return FAT32_OK;
}

fat32_error_t fat32_utils_join_path(const char *dir_path,
                                    const char *filename,
                                    char *full_path,
                                    size_t max_len)
{
    avatar_assert(dir_path != NULL);
    avatar_assert(filename != NULL);
    avatar_assert(full_path != NULL);
    
    size_t dir_len = strlen(dir_path);
    size_t name_len = strlen(filename);
    
    // 计算所需的总长度
    size_t total_len = dir_len + name_len + 2;  // +2 for '/' and '\0'
    
    if (total_len > max_len) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    strcpy(full_path, dir_path);
    
    // 添加路径分隔符（如果需要）
    if (dir_len > 0 && !fat32_utils_is_path_separator(dir_path[dir_len - 1])) {
        strcat(full_path, "/");
    }
    
    strcat(full_path, filename);
    
    return FAT32_OK;
}

uint8_t fat32_utils_is_absolute_path(const char *path)
{
    avatar_assert(path != NULL);
    
    return (path[0] != '\0' && fat32_utils_is_path_separator(path[0]));
}

fat32_error_t fat32_utils_get_file_extension(const char *filename,
                                             char *extension,
                                             size_t max_len)
{
    avatar_assert(filename != NULL);
    avatar_assert(extension != NULL);
    
    const char *dot_pos = strrchr(filename, '.');
    if (dot_pos == NULL || dot_pos == filename) {
        // 没有扩展名或点号在开头
        extension[0] = '\0';
        return FAT32_OK;
    }
    
    const char *ext_start = dot_pos + 1;
    size_t ext_len = strlen(ext_start);
    
    if (ext_len >= max_len) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    strcpy(extension, ext_start);
    return FAT32_OK;
}

/* ============================================================================
 * 时间和日期转换函数实现
 * ============================================================================ */

fat32_error_t fat32_utils_fat_to_datetime(uint16_t fat_time,
                                          uint16_t fat_date,
                                          fat32_datetime_t *datetime)
{
    avatar_assert(datetime != NULL);
    
    // 解析FAT32日期格式：YYYYYYYMMMMDDDDD
    datetime->year = 1980 + ((fat_date >> 9) & 0x7F);
    datetime->month = (fat_date >> 5) & 0x0F;
    datetime->day = fat_date & 0x1F;
    
    // 解析FAT32时间格式：HHHHHMMMMMMSSSSSS（秒为2秒精度）
    datetime->hour = (fat_time >> 11) & 0x1F;
    datetime->minute = (fat_time >> 5) & 0x3F;
    datetime->second = (fat_time & 0x1F) * 2;
    
    // 基本有效性检查
    if (datetime->month == 0 || datetime->month > 12 ||
        datetime->day == 0 || datetime->day > 31 ||
        datetime->hour > 23 || datetime->minute > 59 || datetime->second > 59) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    return FAT32_OK;
}

fat32_error_t fat32_utils_datetime_to_fat(const fat32_datetime_t *datetime,
                                          uint16_t *fat_time,
                                          uint16_t *fat_date)
{
    avatar_assert(datetime != NULL);
    avatar_assert(fat_time != NULL);
    avatar_assert(fat_date != NULL);
    
    // 基本有效性检查
    if (datetime->year < 1980 || datetime->year > 2107 ||
        datetime->month == 0 || datetime->month > 12 ||
        datetime->day == 0 || datetime->day > 31 ||
        datetime->hour > 23 || datetime->minute > 59 || datetime->second > 59) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    // 构造FAT32日期格式
    *fat_date = ((datetime->year - 1980) << 9) |
                (datetime->month << 5) |
                datetime->day;
    
    // 构造FAT32时间格式（秒除以2）
    *fat_time = (datetime->hour << 11) |
                (datetime->minute << 5) |
                (datetime->second / 2);
    
    return FAT32_OK;
}

fat32_error_t fat32_utils_get_current_time(fat32_datetime_t *datetime)
{
    avatar_assert(datetime != NULL);
    
    // 简化实现：返回固定的时间值
    // 实际实现中应该获取系统时间
    datetime->year = 2024;
    datetime->month = 1;
    datetime->day = 1;
    datetime->hour = 12;
    datetime->minute = 0;
    datetime->second = 0;
    
    return FAT32_OK;
}

/* ============================================================================
 * 数据格式转换函数实现
 * ============================================================================ */

uint16_t fat32_utils_le16_to_host(uint16_t le_value)
{
    // 假设主机是小端序（大多数现代系统）
    return le_value;
}

uint16_t fat32_utils_host_to_le16(uint16_t host_value)
{
    // 假设主机是小端序
    return host_value;
}

uint32_t fat32_utils_le32_to_host(uint32_t le_value)
{
    // 假设主机是小端序
    return le_value;
}

uint32_t fat32_utils_host_to_le32(uint32_t host_value)
{
    // 假设主机是小端序
    return host_value;
}

/* ============================================================================
 * 校验和计算函数实现
 * ============================================================================ */

uint8_t fat32_utils_calculate_checksum(const uint8_t *short_name)
{
    avatar_assert(short_name != NULL);
    
    uint8_t checksum = 0;
    for (int i = 0; i < 11; i++) {
        checksum = ((checksum & 1) << 7) + (checksum >> 1) + short_name[i];
    }
    
    return checksum;
}

uint8_t fat32_utils_verify_checksum(const uint8_t *short_name, uint8_t checksum)
{
    avatar_assert(short_name != NULL);
    
    return (fat32_utils_calculate_checksum(short_name) == checksum);
}

/* ============================================================================
 * 调试辅助函数实现
 * ============================================================================ */

void fat32_utils_print_hex(const void *data, size_t size, const char *prefix)
{
    avatar_assert(data != NULL);
    
    const uint8_t *bytes = (const uint8_t *)data;
    
    if (prefix != NULL) {
        logger("%s", prefix);
    }
    
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) {
            logger("\n%04zx: ", i);
        }
        logger("%02x ", bytes[i]);
    }
    logger("\n");
}

void fat32_utils_print_dir_entry(const fat32_dir_entry_t *dir_entry)
{
    avatar_assert(dir_entry != NULL);
    
    char filename[13];
    fat32_dir_convert_from_dir_entry(dir_entry, filename, sizeof(filename));
    
    fat32_datetime_t create_time, write_time;
    fat32_utils_fat_to_datetime(dir_entry->create_time, dir_entry->create_date, &create_time);
    fat32_utils_fat_to_datetime(dir_entry->write_time, dir_entry->write_date, &write_time);
    
    logger("File: %s\n", filename);
    logger("  Size: %u bytes\n", dir_entry->file_size);
    logger("  First cluster: %u\n", fat32_dir_get_first_cluster(dir_entry));
    logger("  Attributes: 0x%02x ", dir_entry->attr);
    fat32_utils_print_file_attributes(dir_entry->attr);
    logger("  Created: %04u-%02u-%02u %02u:%02u:%02u\n",
           create_time.year, create_time.month, create_time.day,
           create_time.hour, create_time.minute, create_time.second);
    logger("  Modified: %04u-%02u-%02u %02u:%02u:%02u\n",
           write_time.year, write_time.month, write_time.day,
           write_time.hour, write_time.minute, write_time.second);
}

void fat32_utils_print_file_attributes(uint8_t attr)
{
    logger("(");
    if (attr & FAT32_ATTR_READ_ONLY) logger("R");
    if (attr & FAT32_ATTR_HIDDEN) logger("H");
    if (attr & FAT32_ATTR_SYSTEM) logger("S");
    if (attr & FAT32_ATTR_VOLUME_ID) logger("V");
    if (attr & FAT32_ATTR_DIRECTORY) logger("D");
    if (attr & FAT32_ATTR_ARCHIVE) logger("A");
    logger(")\n");
}

fat32_error_t fat32_utils_format_file_size(uint32_t size,
                                           char *formatted_size,
                                           size_t max_len)
{
    avatar_assert(formatted_size != NULL);
    
    const char *units[] = {"B", "KB", "MB", "GB"};
    const uint32_t unit_size[] = {1, 1024, 1024*1024, 1024*1024*1024};
    
    int unit_index = 0;
    uint32_t display_size = size;
    
    // 选择合适的单位
    for (int i = 3; i >= 0; i--) {
        if (size >= unit_size[i]) {
            unit_index = i;
            display_size = size / unit_size[i];
            break;
        }
    }
    
    // 格式化字符串
    int result = my_snprintf(formatted_size, max_len, "%u %s", display_size, units[unit_index]);
    if (result < 0 || (size_t)result >= max_len) {
        return FAT32_ERROR_INVALID_PARAM;
    }
    
    return FAT32_OK;
}
