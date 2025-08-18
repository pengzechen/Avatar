/**
 * @file fat32_file.h
 * @brief FAT32文件操作头文件
 * 
 * 本文件定义了FAT32文件系统的文件操作功能，包括：
 * - 文件的打开、关闭、读写
 * - 文件的创建和删除
 * - 文件指针的定位
 * - 文件属性的管理
 * 
 * 简化说明：当前实现支持基本的文件操作，不包含复杂的并发控制
 */

#ifndef FAT32_FILE_H
#define FAT32_FILE_H

#include "fat32_types.h"
#include "fat32_disk.h"

/* ============================================================================
 * 文件操作常量
 * ============================================================================ */

#define FAT32_MAX_OPEN_FILES 32  // 最大同时打开的文件数

/* 文件打开标志 */
#define FAT32_O_RDONLY 0x01  // 只读
#define FAT32_O_WRONLY 0x02  // 只写
#define FAT32_O_RDWR   0x03  // 读写
#define FAT32_O_CREAT  0x04  // 创建文件
#define FAT32_O_TRUNC  0x08  // 截断文件
#define FAT32_O_APPEND 0x10  // 追加模式

/* 文件定位标志 */
#define FAT32_SEEK_SET 0  // 从文件开头
#define FAT32_SEEK_CUR 1  // 从当前位置
#define FAT32_SEEK_END 2  // 从文件末尾

/* ============================================================================
 * 文件操作函数
 * ============================================================================ */

/**
 * @brief 打开文件
 * 
 * 打开指定路径的文件，返回文件句柄。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param filepath 文件路径
 * @param flags 打开标志
 * @param file_handle 返回的文件句柄指针
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 解析文件路径，定位文件
 * - 根据标志创建或打开文件
 * - 分配文件句柄并初始化
 */
fat32_error_t
fat32_file_open(fat32_disk_t         *disk,
                fat32_fs_info_t      *fs_info,
                const char           *filepath,
                uint32_t              flags,
                fat32_file_handle_t **file_handle);

/**
 * @brief 关闭文件
 * 
 * 关闭文件句柄，释放相关资源。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param file_handle 文件句柄
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 刷新缓存数据到磁盘
 * - 更新文件大小和时间戳
 * - 释放文件句柄
 */
fat32_error_t
fat32_file_close(fat32_disk_t *disk, fat32_fs_info_t *fs_info, fat32_file_handle_t *file_handle);

/**
 * @brief 读取文件数据
 * 
 * 从文件当前位置读取指定数量的数据。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param file_handle 文件句柄
 * @param buffer 数据缓冲区
 * @param size 要读取的字节数
 * @param bytes_read 返回实际读取的字节数
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_read(fat32_disk_t          *disk,
                const fat32_fs_info_t *fs_info,
                fat32_file_handle_t   *file_handle,
                void                  *buffer,
                uint32_t               size,
                uint32_t              *bytes_read);

/**
 * @brief 写入文件数据
 * 
 * 向文件当前位置写入指定数量的数据。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param file_handle 文件句柄
 * @param buffer 数据缓冲区
 * @param size 要写入的字节数
 * @param bytes_written 返回实际写入的字节数
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_write(fat32_disk_t        *disk,
                 fat32_fs_info_t     *fs_info,
                 fat32_file_handle_t *file_handle,
                 const void          *buffer,
                 uint32_t             size,
                 uint32_t            *bytes_written);

/**
 * @brief 定位文件指针
 * 
 * 设置文件的当前读写位置。
 * 
 * @param file_handle 文件句柄
 * @param offset 偏移量
 * @param whence 定位标志
 * @param new_position 返回新的文件位置
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_seek(fat32_file_handle_t *file_handle,
                int32_t              offset,
                int                  whence,
                uint32_t            *new_position);

/**
 * @brief 获取文件当前位置
 * 
 * @param file_handle 文件句柄
 * @return uint32_t 当前文件位置
 */
uint32_t
fat32_file_tell(const fat32_file_handle_t *file_handle);

/**
 * @brief 刷新文件缓存
 * 
 * 将文件的缓存数据写入磁盘。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param file_handle 文件句柄
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_flush(fat32_disk_t *disk, fat32_fs_info_t *fs_info, fat32_file_handle_t *file_handle);

/**
 * @brief 截断文件
 * 
 * 将文件截断到指定大小。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param file_handle 文件句柄
 * @param new_size 新的文件大小
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_truncate(fat32_disk_t        *disk,
                    fat32_fs_info_t     *fs_info,
                    fat32_file_handle_t *file_handle,
                    uint32_t             new_size);

/* ============================================================================
 * 文件管理函数
 * ============================================================================ */

/**
 * @brief 创建文件
 * 
 * 在指定目录中创建新文件。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param filepath 文件路径
 * @param attr 文件属性
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_create(fat32_disk_t *disk, fat32_fs_info_t *fs_info, const char *filepath, uint8_t attr);

/**
 * @brief 删除文件
 * 
 * 删除指定路径的文件。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param filepath 文件路径
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_delete(fat32_disk_t *disk, fat32_fs_info_t *fs_info, const char *filepath);

/**
 * @brief 重命名文件
 * 
 * 将文件从旧路径重命名到新路径。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param old_path 旧文件路径
 * @param new_path 新文件路径
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_rename(fat32_disk_t    *disk,
                  fat32_fs_info_t *fs_info,
                  const char      *old_path,
                  const char      *new_path);

/**
 * @brief 获取文件信息
 * 
 * 获取文件的详细信息。
 * 
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param filepath 文件路径
 * @param dir_entry 返回的目录项信息
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_stat(fat32_disk_t          *disk,
                const fat32_fs_info_t *fs_info,
                const char            *filepath,
                fat32_dir_entry_t     *dir_entry);

/* ============================================================================
 * 文件句柄管理函数
 * ============================================================================ */

/**
 * @brief 初始化文件句柄管理器
 * 
 * 初始化全局文件句柄池。
 * 
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_handle_init(void);

/**
 * @brief 分配文件句柄
 * 
 * 从句柄池中分配一个空闲的文件句柄。
 * 
 * @param file_handle 返回的文件句柄指针
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_handle_alloc(fat32_file_handle_t **file_handle);

/**
 * @brief 释放文件句柄
 * 
 * 将文件句柄返回到句柄池。
 * 
 * @param file_handle 要释放的文件句柄
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_handle_free(fat32_file_handle_t *file_handle);

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */

/**
 * @brief 检查文件句柄是否有效
 * 
 * @param file_handle 文件句柄
 * @return uint8_t 1表示有效，0表示无效
 */
static inline uint8_t
fat32_file_is_valid_handle(const fat32_file_handle_t *file_handle)
{
    return (file_handle != NULL && file_handle->in_use);
}

/**
 * @brief 检查文件是否可读
 * 
 * @param file_handle 文件句柄
 * @return uint8_t 1表示可读，0表示不可读
 */
static inline uint8_t
fat32_file_is_readable(const fat32_file_handle_t *file_handle)
{
    return (file_handle->flags & (FAT32_O_RDONLY | FAT32_O_RDWR)) != 0;
}

/**
 * @brief 检查文件是否可写
 * 
 * @param file_handle 文件句柄
 * @return uint8_t 1表示可写，0表示不可写
 */
static inline uint8_t
fat32_file_is_writable(const fat32_file_handle_t *file_handle)
{
    return (file_handle->flags & (FAT32_O_WRONLY | FAT32_O_RDWR)) != 0;
}

/**
 * @brief 检查是否到达文件末尾
 * 
 * @param file_handle 文件句柄
 * @return uint8_t 1表示到达末尾，0表示未到达
 */
static inline uint8_t
fat32_file_is_eof(const fat32_file_handle_t *file_handle)
{
    return (file_handle->file_position >= file_handle->file_size);
}

/* ============================================================================
 * 路径处理函数
 * ============================================================================ */

/**
 * @brief 解析文件路径
 *
 * 将完整的文件路径分解为目录路径和文件名。
 *
 * @param filepath 完整文件路径
 * @param dirname 输出的目录路径
 * @param filename 输出的文件名
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_parse_path(const char *filepath, char *dirname, char *filename);

/**
 * @brief 查找目录
 *
 * 根据目录路径查找对应的目录簇号。
 *
 * @param disk 磁盘句柄
 * @param fs_info 文件系统信息
 * @param dirpath 目录路径
 * @param dir_cluster 输出的目录簇号
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_file_find_directory(fat32_disk_t          *disk,
                          const fat32_fs_info_t *fs_info,
                          const char            *dirpath,
                          uint32_t              *dir_cluster);

#endif  // FAT32_FILE_H
