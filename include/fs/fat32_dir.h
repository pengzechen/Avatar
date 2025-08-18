/**
 * @file fat32_dir.h
 * @brief FAT32目录操作头文件
 * 
 * 本文件定义了FAT32目录的操作功能，包括：
 * - 目录的创建和删除
 * - 目录项的读取和写入
 * - 目录的遍历和搜索
 * - 文件名的处理和转换
 * 
 * 简化说明：当前实现主要支持短文件名（8.3格式），长文件名支持有限
 */

#ifndef FAT32_DIR_H
#define FAT32_DIR_H

#include "fat32_types.h"
#include "fat32_disk.h"

/* ============================================================================
 * 目录操作常量
 * ============================================================================ */

#define FAT32_DIR_ENTRY_FREE    0x00    // 空闲目录项
#define FAT32_DIR_ENTRY_DELETED 0xE5    // 已删除目录项
#define FAT32_DIR_ENTRY_DOT     0x2E    // "." 目录项
#define FAT32_DIR_ENTRY_DOTDOT  0x2E2E  // ".." 目录项

/* ============================================================================
 * 目录遍历结构
 * ============================================================================ */

/**
 * @brief 目录遍历上下文
 * 
 * 用于遍历目录时保存状态信息
 */
typedef struct
{
    uint32_t dir_cluster;      // 目录起始簇号
    uint32_t current_cluster;  // 当前遍历的簇号
    uint32_t entry_index;      // 当前目录项索引
    uint32_t cluster_offset;   // 在当前簇内的偏移
    uint8_t  end_of_dir;       // 是否到达目录末尾
} fat32_dir_iterator_t;

/* ============================================================================
 * 目录操作函数
 * ============================================================================ */

/**
 * @brief 读取目录项
 * 
 * 从指定位置读取一个目录项。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param dir_cluster 目录起始簇号
 * @param entry_index 目录项索引
 * @param dir_entry 返回的目录项数据
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_read_entry(fat32_disk_t          *disk,
                     const fat32_fs_info_t *fs_info,
                     uint32_t               dir_cluster,
                     uint32_t               entry_index,
                     fat32_dir_entry_t     *dir_entry);

/**
 * @brief 写入目录项
 * 
 * 向指定位置写入一个目录项。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param dir_cluster 目录起始簇号
 * @param entry_index 目录项索引
 * @param dir_entry 要写入的目录项数据
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_write_entry(fat32_disk_t            *disk,
                      const fat32_fs_info_t   *fs_info,
                      uint32_t                 dir_cluster,
                      uint32_t                 entry_index,
                      const fat32_dir_entry_t *dir_entry);

/**
 * @brief 在目录中查找文件
 * 
 * 在指定目录中搜索具有给定名称的文件或子目录。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param dir_cluster 目录起始簇号
 * @param filename 要查找的文件名
 * @param dir_entry 返回找到的目录项
 * @param entry_index 返回目录项索引
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_find_entry(fat32_disk_t          *disk,
                     const fat32_fs_info_t *fs_info,
                     uint32_t               dir_cluster,
                     const char            *filename,
                     fat32_dir_entry_t     *dir_entry,
                     uint32_t              *entry_index);

/**
 * @brief 在目录中创建新的目录项
 * 
 * 在指定目录中创建一个新的文件或子目录项。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param dir_cluster 目录起始簇号
 * @param filename 文件名
 * @param attr 文件属性
 * @param first_cluster 文件起始簇号
 * @param file_size 文件大小
 * @param entry_index 返回新目录项的索引
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_create_entry(fat32_disk_t    *disk,
                       fat32_fs_info_t *fs_info,
                       uint32_t         dir_cluster,
                       const char      *filename,
                       uint8_t          attr,
                       uint32_t         first_cluster,
                       uint32_t         file_size,
                       uint32_t        *entry_index);

/**
 * @brief 删除目录项
 * 
 * 将指定的目录项标记为已删除。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param dir_cluster 目录起始簇号
 * @param entry_index 要删除的目录项索引
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_delete_entry(fat32_disk_t          *disk,
                       const fat32_fs_info_t *fs_info,
                       uint32_t               dir_cluster,
                       uint32_t               entry_index);

/**
 * @brief 创建目录
 * 
 * 创建一个新的子目录，包括分配簇和创建"."和".."目录项。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param parent_cluster 父目录簇号
 * @param dirname 目录名
 * @param new_dir_cluster 返回新目录的起始簇号
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_create_directory(fat32_disk_t    *disk,
                           fat32_fs_info_t *fs_info,
                           uint32_t         parent_cluster,
                           const char      *dirname,
                           uint32_t        *new_dir_cluster);

/**
 * @brief 删除目录
 * 
 * 删除一个空的子目录。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param parent_cluster 父目录簇号
 * @param dirname 目录名
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_remove_directory(fat32_disk_t    *disk,
                           fat32_fs_info_t *fs_info,
                           uint32_t         parent_cluster,
                           const char      *dirname);

/* ============================================================================
 * 目录遍历函数
 * ============================================================================ */

/**
 * @brief 初始化目录遍历器
 * 
 * @param iterator 目录遍历器
 * @param dir_cluster 目录起始簇号
 */
void
fat32_dir_iterator_init(fat32_dir_iterator_t *iterator, uint32_t dir_cluster);

/**
 * @brief 获取下一个目录项
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param iterator 目录遍历器
 * @param dir_entry 返回的目录项
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_iterator_next(fat32_disk_t          *disk,
                        const fat32_fs_info_t *fs_info,
                        fat32_dir_iterator_t  *iterator,
                        fat32_dir_entry_t     *dir_entry);

/**
 * @brief 检查目录是否为空
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param dir_cluster 目录起始簇号
 * @param is_empty 返回是否为空
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_is_empty(fat32_disk_t          *disk,
                   const fat32_fs_info_t *fs_info,
                   uint32_t               dir_cluster,
                   uint8_t               *is_empty);

/* ============================================================================
 * 文件名处理函数
 * ============================================================================ */

/**
 * @brief 将长文件名转换为8.3格式
 * 
 * @param long_name 长文件名
 * @param short_name 返回的8.3格式文件名（11字节）
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_convert_to_short_name(const char *long_name, uint8_t *short_name);

/**
 * @brief 将8.3格式文件名转换为普通字符串
 * 
 * @param short_name 8.3格式文件名（11字节）
 * @param long_name 返回的普通文件名
 * @param max_len 缓冲区最大长度
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_convert_from_short_name(const uint8_t *short_name, char *long_name, size_t max_len);

/**
 * @brief 从目录项转换文件名（支持大小写处理）
 *
 * @param dir_entry 目录项
 * @param long_name 返回的文件名
 * @param max_len 缓冲区最大长度
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_dir_convert_from_dir_entry(const fat32_dir_entry_t *dir_entry,
                                 char                    *long_name,
                                 size_t                   max_len);

/**
 * @brief 比较两个8.3格式文件名
 * 
 * @param name1 第一个文件名
 * @param name2 第二个文件名
 * @return int 0表示相等，非0表示不等
 */
int
fat32_dir_compare_short_names(const uint8_t *name1, const uint8_t *name2);

/**
 * @brief 验证文件名是否有效
 * 
 * @param filename 文件名
 * @return uint8_t 1表示有效，0表示无效
 */
uint8_t
fat32_dir_is_valid_filename(const char *filename);

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */

/**
 * @brief 检查目录项是否为空闲
 * 
 * @param dir_entry 目录项
 * @return uint8_t 1表示空闲，0表示已使用
 */
static inline uint8_t
fat32_dir_is_free_entry(const fat32_dir_entry_t *dir_entry)
{
    return (dir_entry->name[0] == FAT32_DIR_ENTRY_FREE);
}

/**
 * @brief 检查目录项是否已删除
 * 
 * @param dir_entry 目录项
 * @return uint8_t 1表示已删除，0表示正常
 */
static inline uint8_t
fat32_dir_is_deleted_entry(const fat32_dir_entry_t *dir_entry)
{
    return (dir_entry->name[0] == FAT32_DIR_ENTRY_DELETED);
}

/**
 * @brief 检查目录项是否为长文件名目录项
 *
 * @param dir_entry 目录项
 * @return uint8_t 1表示是长文件名目录项，0表示不是
 */
static inline uint8_t
fat32_dir_is_long_name_entry(const fat32_dir_entry_t *dir_entry)
{
    return (dir_entry->attr == FAT32_ATTR_LONG_NAME);
}

/**
 * @brief 检查目录项是否为目录
 * 
 * @param dir_entry 目录项
 * @return uint8_t 1表示是目录，0表示是文件
 */
static inline uint8_t
fat32_dir_is_directory(const fat32_dir_entry_t *dir_entry)
{
    return (dir_entry->attr & FAT32_ATTR_DIRECTORY) != 0;
}

/**
 * @brief 获取目录项的起始簇号
 * 
 * @param dir_entry 目录项
 * @return uint32_t 起始簇号
 */
static inline uint32_t
fat32_dir_get_first_cluster(const fat32_dir_entry_t *dir_entry)
{
    return ((uint32_t) dir_entry->first_cluster_high << 16) | dir_entry->first_cluster_low;
}

/**
 * @brief 设置目录项的起始簇号
 * 
 * @param dir_entry 目录项
 * @param cluster_num 簇号
 */
static inline void
fat32_dir_set_first_cluster(fat32_dir_entry_t *dir_entry, uint32_t cluster_num)
{
    dir_entry->first_cluster_high = (uint16_t) (cluster_num >> 16);
    dir_entry->first_cluster_low  = (uint16_t) (cluster_num & 0xFFFF);
}

#endif  // FAT32_DIR_H
