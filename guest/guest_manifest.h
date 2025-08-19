#ifndef _GUEST_MANIFEST_H
#define _GUEST_MANIFEST_H

#include "avatar_types.h"

// Guest 类型枚举
typedef enum {
    GUEST_TYPE_LINUX = 0,
    GUEST_TYPE_NIMBOS,
    GUEST_TYPE_TESTOS,
    GUEST_TYPE_MAX
} guest_type_t;

// Guest 加载错误码
typedef enum {
    GUEST_LOAD_SUCCESS = 0,
    GUEST_LOAD_ERROR_INVALID_PARAM,
    GUEST_LOAD_ERROR_KERNEL_LOAD_FAILED,
    GUEST_LOAD_ERROR_DTB_LOAD_FAILED,
    GUEST_LOAD_ERROR_INITRD_LOAD_FAILED,
    GUEST_LOAD_ERROR_NO_MEMORY,
    GUEST_LOAD_ERROR_FILE_SYSTEM_ERROR
} guest_load_error_t;

// Guest 文件路径配置
typedef struct {
    const char *kernel_path;    // 内核文件路径
    const char *dtb_path;       // DTB文件路径（可选）
    const char *initrd_path;    // initrd文件路径（可选）
    bool        needs_dtb;      // 是否需要DTB
    bool        needs_initrd;   // 是否需要initrd
} guest_files_t;

// Guest 运行时配置
typedef struct {
    guest_type_t    type;           // Guest类型
    const char     *name;           // Guest名称
    uint64_t        bin_loadaddr;   // 内核加载地址
    uint64_t        dtb_loadaddr;   // DTB加载地址
    uint64_t        fs_loadaddr;    // initrd加载地址
    uint32_t        smp_num;        // vCPU数量
    guest_files_t   files;          // 文件路径配置
} guest_manifest_t;

// Guest 加载结果
typedef struct {
    guest_load_error_t error;       // 错误码
    size_t            kernel_size;  // 已加载的内核大小
    size_t            dtb_size;     // 已加载的DTB大小
    size_t            initrd_size;  // 已加载的initrd大小
} guest_load_result_t;

// 预定义的Guest配置
extern const guest_manifest_t guest_manifests[];
extern const uint32_t guest_manifest_count;

// 函数声明
const char* guest_load_error_to_string(guest_load_error_t error);
const char* guest_type_to_string(guest_type_t type);

#endif // _GUEST_MANIFEST_H
