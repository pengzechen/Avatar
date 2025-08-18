/**
 * @file fat32_types.h
 * @brief FAT32文件系统基础数据结构定义
 * 
 * 本文件定义了FAT32文件系统的所有基础数据结构，包括：
 * - 引导扇区结构
 * - FAT表项定义
 * - 目录项结构
 * - 文件系统元数据
 * 
 * 注意：为了简化实现，某些字段可能不会完全使用，但保留了完整的结构定义
 * 以便于理解FAT32的标准格式
 */

#ifndef FAT32_TYPES_H
#define FAT32_TYPES_H

#include "avatar_types.h"

/* ============================================================================
 * 基础常量定义
 * ============================================================================ */

#define FAT32_SECTOR_SIZE        512   // 扇区大小（字节）
#define FAT32_MAX_FILENAME       255   // 最大文件名长度
#define FAT32_MAX_PATH           1024  // 最大路径长度
#define FAT32_DIR_ENTRY_SIZE     32    // 目录项大小（字节）
#define FAT32_ENTRIES_PER_SECTOR 16    // 每扇区目录项数量

/* FAT表项特殊值 */
#define FAT32_FREE_CLUSTER 0x00000000  // 空闲簇
#define FAT32_RESERVED_MIN 0x00000002  // 保留簇最小值
#define FAT32_RESERVED_MAX 0x0FFFFFEF  // 保留簇最大值
#define FAT32_BAD_CLUSTER  0x0FFFFFF7  // 坏簇标记
#define FAT32_EOC_MIN      0x0FFFFFF8  // 簇链结束标记最小值
#define FAT32_EOC_MAX      0x0FFFFFFF  // 簇链结束标记最大值

/* 文件属性标志 */
#define FAT32_ATTR_READ_ONLY 0x01  // 只读
#define FAT32_ATTR_HIDDEN    0x02  // 隐藏
#define FAT32_ATTR_SYSTEM    0x04  // 系统文件
#define FAT32_ATTR_VOLUME_ID 0x08  // 卷标
#define FAT32_ATTR_DIRECTORY 0x10  // 目录
#define FAT32_ATTR_ARCHIVE   0x20  // 归档
#define FAT32_ATTR_LONG_NAME 0x0F  // 长文件名（组合属性）

/* ============================================================================
 * 引导扇区结构（Boot Sector）
 * ============================================================================ */

/**
 * @brief FAT32引导扇区结构
 * 
 * 位于磁盘的第0扇区，包含文件系统的基本参数信息
 * 简化说明：我们主要关注文件系统布局相关的字段
 */
typedef struct __attribute__((packed))
{
    uint8_t  jmp_boot[3];          // 跳转指令（0xEB, 0x??, 0x90）
    uint8_t  oem_name[8];          // OEM名称
    uint16_t bytes_per_sector;     // 每扇区字节数（通常512）
    uint8_t  sectors_per_cluster;  // 每簇扇区数（必须是2的幂）
    uint16_t reserved_sectors;     // 保留扇区数（包括引导扇区）
    uint8_t  num_fats;             // FAT表数量（通常为2）
    uint16_t root_entries;         // 根目录项数（FAT32中为0）
    uint16_t total_sectors_16;     // 总扇区数（小于65536时使用）
    uint8_t  media_type;           // 媒体类型（0xF8表示硬盘）
    uint16_t fat_size_16;          // FAT表大小（FAT32中为0）
    uint16_t sectors_per_track;    // 每磁道扇区数
    uint16_t num_heads;            // 磁头数
    uint32_t hidden_sectors;       // 隐藏扇区数
    uint32_t total_sectors_32;     // 总扇区数（大于等于65536时使用）

    /* FAT32特有字段 */
    uint32_t fat_size_32;            // FAT表大小（扇区数）
    uint16_t ext_flags;              // 扩展标志
    uint16_t fs_version;             // 文件系统版本
    uint32_t root_cluster;           // 根目录起始簇号
    uint16_t fs_info;                // FSInfo结构扇区号
    uint16_t backup_boot_sector;     // 备份引导扇区位置
    uint8_t  reserved[12];           // 保留字段
    uint8_t  drive_number;           // 驱动器号
    uint8_t  reserved1;              // 保留
    uint8_t  boot_signature;         // 引导签名（0x29）
    uint32_t volume_id;              // 卷序列号
    uint8_t  volume_label[11];       // 卷标
    uint8_t  fs_type[8];             // 文件系统类型（"FAT32   "）
    uint8_t  boot_code[420];         // 引导代码
    uint16_t boot_sector_signature;  // 引导扇区签名（0xAA55）
} fat32_boot_sector_t;

/* ============================================================================
 * FSInfo结构
 * ============================================================================ */

/**
 * @brief FSInfo结构
 * 
 * 包含文件系统的状态信息，如空闲簇数量等
 * 简化说明：主要用于优化空闲簇的查找
 */
typedef struct __attribute__((packed))
{
    uint32_t lead_signature;    // 前导签名（0x41615252）
    uint8_t  reserved1[480];    // 保留字段
    uint32_t struct_signature;  // 结构签名（0x61417272）
    uint32_t free_count;        // 空闲簇数量（0xFFFFFFFF表示未知）
    uint32_t next_free;         // 下一个空闲簇号提示
    uint8_t  reserved2[12];     // 保留字段
    uint32_t trail_signature;   // 尾部签名（0xAA550000）
} fat32_fsinfo_t;

/* ============================================================================
 * 目录项结构
 * ============================================================================ */

/**
 * @brief 短文件名目录项结构
 * 
 * 标准的8.3格式文件名目录项
 * 简化说明：我们主要实现短文件名支持，长文件名作为扩展功能
 */
typedef struct __attribute__((packed))
{
    uint8_t  name[11];            // 文件名（8字节名称+3字节扩展名）
    uint8_t  attr;                // 文件属性
    uint8_t  nt_reserved;         // NT保留字段
    uint8_t  create_time_tenth;   // 创建时间（十分之一秒）
    uint16_t create_time;         // 创建时间
    uint16_t create_date;         // 创建日期
    uint16_t last_access_date;    // 最后访问日期
    uint16_t first_cluster_high;  // 起始簇号高16位
    uint16_t write_time;          // 修改时间
    uint16_t write_date;          // 修改日期
    uint16_t first_cluster_low;   // 起始簇号低16位
    uint32_t file_size;           // 文件大小（字节）
} fat32_dir_entry_t;

/**
 * @brief 长文件名目录项结构
 * 
 * 用于支持长文件名的目录项
 * 简化说明：当前实现中可能不完全支持，但保留结构定义
 */
typedef struct __attribute__((packed))
{
    uint8_t  order;              // 序号（最后一个条目的序号|0x40）
    uint16_t name1[5];           // 文件名第1-5个字符（Unicode）
    uint8_t  attr;               // 属性（必须为0x0F）
    uint8_t  type;               // 类型（必须为0）
    uint8_t  checksum;           // 短文件名校验和
    uint16_t name2[6];           // 文件名第6-11个字符（Unicode）
    uint16_t first_cluster_low;  // 起始簇号（必须为0）
    uint16_t name3[2];           // 文件名第12-13个字符（Unicode）
} fat32_lfn_entry_t;

/* ============================================================================
 * 文件系统运行时结构
 * ============================================================================ */

/**
 * @brief FAT32文件系统信息结构
 * 
 * 运行时维护的文件系统状态信息
 */
typedef struct
{
    fat32_boot_sector_t boot_sector;  // 引导扇区副本
    fat32_fsinfo_t      fsinfo;       // FSInfo结构副本

    /* 计算得出的文件系统参数 */
    uint32_t fat_start_sector;     // FAT表起始扇区
    uint32_t data_start_sector;    // 数据区起始扇区
    uint32_t total_clusters;       // 总簇数
    uint32_t sectors_per_cluster;  // 每簇扇区数
    uint32_t bytes_per_cluster;    // 每簇字节数

    /* 运行时状态 */
    uint32_t next_free_cluster;   // 下一个可能的空闲簇
    uint32_t free_cluster_count;  // 空闲簇数量
    uint8_t  mounted;             // 挂载状态标志
} fat32_fs_info_t;

/**
 * @brief 文件句柄结构
 * 
 * 表示一个打开的文件或目录
 */
typedef struct
{
    uint32_t first_cluster;                 // 文件起始簇号
    uint32_t current_cluster;               // 当前访问的簇号
    uint32_t cluster_offset;                // 在当前簇内的偏移
    uint32_t file_size;                     // 文件大小
    uint32_t file_position;                 // 当前文件位置
    uint8_t  attr;                          // 文件属性
    uint8_t  flags;                         // 打开标志（读/写/追加等）
    uint8_t  in_use;                        // 句柄是否在使用中
    uint8_t  modified;                      // 文件是否被修改过
    uint32_t dir_cluster;                   // 文件所在目录的簇号
    uint32_t dir_entry_index;               // 文件在目录中的索引
    char     filename[FAT32_MAX_FILENAME];  // 文件名（用于调试）
} fat32_file_handle_t;

/* ============================================================================
 * 错误码定义
 * ============================================================================ */

typedef enum
{
    FAT32_OK = 0,                     // 成功
    FAT32_ERROR_INVALID_PARAM,        // 无效参数
    FAT32_ERROR_NOT_MOUNTED,          // 文件系统未挂载
    FAT32_ERROR_DISK_ERROR,           // 磁盘错误
    FAT32_ERROR_NOT_FOUND,            // 文件/目录未找到
    FAT32_ERROR_ALREADY_EXISTS,       // 文件/目录已存在
    FAT32_ERROR_NO_SPACE,             // 磁盘空间不足
    FAT32_ERROR_ACCESS_DENIED,        // 访问被拒绝
    FAT32_ERROR_INVALID_NAME,         // 无效文件名
    FAT32_ERROR_TOO_MANY_OPEN_FILES,  // 打开文件数过多
    FAT32_ERROR_END_OF_FILE,          // 文件结束
    FAT32_ERROR_DIRECTORY_NOT_EMPTY,  // 目录非空
    FAT32_ERROR_NOT_A_FILE,           // 不是文件
    FAT32_ERROR_NOT_A_DIRECTORY,      // 不是目录
    FAT32_ERROR_CORRUPTED             // 文件系统损坏
} fat32_error_t;

#endif  // FAT32_TYPES_H
