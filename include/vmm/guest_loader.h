/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file guest_loader.h
 * @brief Implementation of guest_loader.h
 * @author Avatar Project Team
 * @date 2024
 */

#ifndef _GUEST_LOADER_H
#define _GUEST_LOADER_H

#include "../guest/guest_manifest.h"
#include "../fs/fat32.h"

/**
 * Guest 动态加载器
 * 
 * 提供从文件系统动态加载Guest镜像的功能，替代编译时链接的方式
 */

/**
 * 从文件系统加载Guest镜像
 * 
 * @param manifest Guest配置清单
 * @return 加载结果，包含错误码和加载的文件大小信息
 */
guest_load_result_t guest_load_from_manifest(const guest_manifest_t *manifest);

/**
 * 从文件系统加载单个文件到指定内存地址
 * 
 * @param filepath 文件路径
 * @param load_addr 目标加载地址
 * @param loaded_size 输出参数，返回实际加载的字节数
 * @return FAT32错误码
 */
fat32_error_t guest_load_file_to_memory(const char *filepath, 
                                        uint64_t load_addr, 
                                        size_t *loaded_size);

/**
 * 验证Guest文件是否存在
 * 
 * @param manifest Guest配置清单
 * @return true如果所有必需文件都存在，false否则
 */
bool guest_validate_files(const guest_manifest_t *manifest);

/**
 * 获取Guest文件的大小信息
 * 
 * @param manifest Guest配置清单
 * @param kernel_size 输出参数，内核文件大小
 * @param dtb_size 输出参数，DTB文件大小（如果存在）
 * @param initrd_size 输出参数，initrd文件大小（如果存在）
 * @return true如果成功获取信息，false否则
 */
bool guest_get_file_sizes(const guest_manifest_t *manifest,
                         size_t *kernel_size,
                         size_t *dtb_size,
                         size_t *initrd_size);

#endif // _GUEST_LOADER_H
