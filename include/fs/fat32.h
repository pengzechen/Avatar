/**
 * @file fat32.h
 * @brief FAT32文件系统主接口头文件
 * 
 * 本文件定义了FAT32文件系统的统一对外接口。
 * 整合了所有子模块的功能，提供完整的文件系统操作接口。
 * 
 * 主要功能：
 * - 文件系统的挂载和卸载
 * - 文件和目录的基本操作
 * - 统一的错误处理和状态管理
 * - 与现有ramfs接口的兼容性
 */

#ifndef FAT32_H
#define FAT32_H

#include "fat32_types.h"
#include "fat32_disk.h"
#include "fat32_boot.h"
#include "fat32_fat.h"
#include "fat32_dir.h"
#include "fat32_file.h"
#include "fat32_cache.h"

/* ============================================================================
 * 文件系统状态结构
 * ============================================================================ */

/**
 * @brief FAT32文件系统全局状态
 */
typedef struct
{
    fat32_disk_t          *disk;         // 磁盘句柄
    fat32_fs_info_t        fs_info;      // 文件系统信息
    fat32_cache_manager_t *cache_mgr;    // 缓存管理器
    uint8_t                initialized;  // 初始化标志
    uint8_t                mounted;      // 挂载状态
} fat32_context_t;

/* ============================================================================
 * 文件系统管理函数
 * ============================================================================ */

/**
 * @brief 初始化FAT32文件系统
 * 
 * 初始化所有子模块，准备文件系统环境。
 * 
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 初始化虚拟磁盘
 * - 初始化缓存管理器
 * - 初始化文件句柄管理器
 * - 设置全局状态
 */
fat32_error_t
fat32_init(void);

/**
 * @brief 格式化并挂载FAT32文件系统
 * 
 * 创建新的FAT32文件系统并挂载。
 * 
 * @param volume_label 卷标（可选，最多11个字符）
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 格式化虚拟磁盘为FAT32
 * - 读取并验证引导扇区
 * - 初始化文件系统信息
 * - 设置挂载状态
 */
fat32_error_t
fat32_format_and_mount(const char *volume_label);

/**
 * @brief 挂载现有的FAT32文件系统
 * 
 * 挂载已存在的FAT32文件系统。
 * 
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 读取并验证引导扇区
 * - 解析文件系统布局
 * - 读取FSInfo信息
 * - 设置挂载状态
 */
fat32_error_t
fat32_mount(void);

/**
 * @brief 卸载FAT32文件系统
 * 
 * 卸载文件系统，刷新缓存并清理资源。
 * 
 * @return fat32_error_t 错误码
 * 
 * 功能说明：
 * - 关闭所有打开的文件
 * - 刷新缓存到磁盘
 * - 更新FSInfo信息
 * - 清理资源
 */
fat32_error_t
fat32_unmount(void);

/**
 * @brief 清理FAT32文件系统
 * 
 * 清理所有资源，释放内存。
 * 
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_cleanup(void);

/* ============================================================================
 * 兼容性接口函数（与ramfs接口兼容）
 * ============================================================================ */

/**
 * @brief 打开文件（兼容接口）
 *
 * @param name 文件名
 * @return int32_t 文件描述符，-1表示失败
 */
int32_t
fat32_open(const char *name);

/**
 * @brief 以只读模式打开文件（兼容接口）
 *
 * @param name 文件名
 * @return int32_t 文件描述符，-1表示失败（文件不存在时不会创建）
 */
int32_t
fat32_open_readonly(const char *name);

/**
 * @brief 关闭文件（兼容接口）
 * 
 * @param fd 文件描述符
 * @return int32_t 0表示成功，-1表示失败
 */
int32_t
fat32_close(int32_t fd);

/**
 * @brief 读取文件（兼容接口）
 * 
 * @param fd 文件描述符
 * @param buf 数据缓冲区
 * @param count 要读取的字节数
 * @return size_t 实际读取的字节数
 */
size_t
fat32_read(int32_t fd, void *buf, size_t count);

/**
 * @brief 写入文件（兼容接口）
 * 
 * @param fd 文件描述符
 * @param buf 数据缓冲区
 * @param count 要写入的字节数
 * @return size_t 实际写入的字节数
 */
size_t
fat32_write(int32_t fd, const void *buf, size_t count);

/**
 * @brief 定位文件指针（兼容接口）
 * 
 * @param fd 文件描述符
 * @param offset 偏移量
 * @param whence 定位标志
 * @return off_t 新的文件位置，-1表示失败
 */
off_t
fat32_lseek(int32_t fd, off_t offset, int32_t whence);

/**
 * @brief 删除文件（兼容接口）
 * 
 * @param name 文件名
 * @return int32_t 0表示成功，-1表示失败
 */
int32_t
fat32_unlink(const char *name);

/* ============================================================================
 * 扩展功能函数
 * ============================================================================ */

/**
 * @brief 创建目录
 * 
 * @param dirname 目录名
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_mkdir(const char *dirname);

/**
 * @brief 删除目录
 * 
 * @param dirname 目录名
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_rmdir(const char *dirname);

/**
 * @brief 列出目录内容
 * 
 * @param dirname 目录名
 * @param entries 返回的目录项数组
 * @param max_entries 最大目录项数量
 * @param entry_count 返回实际目录项数量
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_listdir(const char        *dirname,
              fat32_dir_entry_t *entries,
              uint32_t           max_entries,
              uint32_t          *entry_count);

/**
 * @brief 获取文件信息
 * 
 * @param filepath 文件路径
 * @param file_info 返回的文件信息
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_stat(const char *filepath, fat32_dir_entry_t *file_info);

/**
 * @brief 重命名文件或目录
 * 
 * @param old_name 旧名称
 * @param new_name 新名称
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_rename(const char *old_name, const char *new_name);

/* ============================================================================
 * 调试和诊断函数
 * ============================================================================ */

/**
 * @brief 打印文件系统信息
 * 
 * 输出文件系统的详细信息，用于调试。
 */
void
fat32_print_fs_info(void);

/**
 * @brief 打印缓存统计信息
 * 
 * 输出缓存的使用统计，用于性能分析。
 */
void
fat32_print_cache_stats(void);

/**
 * @brief 检查文件系统一致性
 * 
 * 执行基本的文件系统一致性检查。
 * 
 * @return fat32_error_t 错误码
 */
fat32_error_t
fat32_fsck(void);

/**
 * @brief 测试FAT32文件系统
 * 
 * 执行基本的功能测试。
 */
void
fat32_test(void);

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */

/**
 * @brief 检查文件系统是否已初始化
 * 
 * @return uint8_t 1表示已初始化，0表示未初始化
 */
uint8_t
fat32_is_initialized(void);

/**
 * @brief 检查文件系统是否已挂载
 * 
 * @return uint8_t 1表示已挂载，0表示未挂载
 */
uint8_t
fat32_is_mounted(void);

/**
 * @brief 获取文件系统上下文
 * 
 * @return fat32_context_t* 文件系统上下文指针
 */
fat32_context_t *
fat32_get_context(void);

/**
 * @brief 将FAT32错误码转换为标准错误码
 * 
 * @param fat32_error FAT32错误码
 * @return int32_t 标准错误码（用于兼容接口）
 */
int32_t
fat32_error_to_errno(fat32_error_t fat32_error);

/**
 * @brief 获取错误描述字符串
 * 
 * @param error_code 错误码
 * @return const char* 错误描述字符串
 */
const char *
fat32_get_error_string(fat32_error_t error_code);


extern fat32_context_t g_fat32_context;

#endif  // FAT32_H
