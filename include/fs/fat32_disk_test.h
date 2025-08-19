/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file fat32_disk_test.h
 * @brief Implementation of fat32_disk_test.h
 * @author Avatar Project Team
 * @date 2024
 */

/**
 * @file fat32_disk_test.h
 * @brief FAT32磁盘模块测试程序头文件
 */

#ifndef FAT32_DISK_TEST_H
#define FAT32_DISK_TEST_H

/**
 * @brief 运行完整的FAT32磁盘模块测试套件
 */
void
fat32_disk_run_tests(void);

/**
 * @brief 运行快速的FAT32磁盘模块测试
 */
void
fat32_disk_quick_test(void);

#endif  // FAT32_DISK_TEST_H
