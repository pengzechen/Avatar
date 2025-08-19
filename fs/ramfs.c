/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file ramfs.c
 * @brief Implementation of ramfs.c
 * @author Avatar Project Team
 * @date 2024
 */



#include "mem/mem.h"
#include "lib/avatar_string.h"
#include "avatar_types.h"
#include "avatar_arg.h"
#include "os_cfg.h"
#include "ramfs.h"
#include "io.h"
#include "../app/app.h"

static Head    *fs_head;
static File    *fs_files[MAX_FILES];  // 存储文件的数组
static uint64_t fs_data_ptr;

// 每个块中可以放多少数据
#define BLOCK_DATA_SIZE (BLOCK_SIZE - sizeof(Content))

// 文件系统初始化
void
ramfs_init()
{
    // 初始化数据区域分配指针
    fs_data_ptr = RAM_FS_MEM_START + sizeof(Head) + MAX_FILES * sizeof(File);
    fs_data_ptr = (fs_data_ptr + BLOCK_SIZE - 1) &
                  ~(BLOCK_SIZE - 1);  // 保证 fs_data_ptr 按照 BLOCK_SIZE 对齐
    uint32_t pages = (fs_data_ptr - RAM_FS_MEM_START) / BLOCK_SIZE;
    logger("alloc %d pages memory for ramfs basic data\n", pages);
    // 进行内存分配，确保我们为文件系统的数据区域分配足够的内存
    pmm_alloc_pages_fs(&g_pmm, pages);

    // 初始化指针
    fs_head        = (Head *) (void *) (RAM_FS_MEM_START);
    fs_head->magic = 9270682;
    // 为每个文件分配内存
    for (int32_t i = 0; i < MAX_FILES; i++) {
        fs_files[i] = (File *) (RAM_FS_MEM_START + sizeof(Head) + i * sizeof(File));
        // 初始化每个文件的指针为NULL
        // memset(fs_files[i], 0, sizeof(File));
    }
}

// 打开文件
int32_t
ramfs_open(const char *name)
{
    // 查找文件是否已存在
    for (int32_t i = 0; i < MAX_FILES; i++) {
        if (strcmp((const char *) fs_files[i]->name, name) == 0) {
            fs_files[i]->current_offset = 0;  // 重置偏移量
            return i;                         // 返回文件描述符
        }
    }

    // 如果文件不存在，并且文件数量未满
    for (int32_t i = 0; i < MAX_FILES; i++) {
        // 找到一个空槽位，初始化新文件
        if (fs_files[i]->name[0] == 0) {  // 文件名为空，表示该位置尚未使用
            // 初始化新文件
            memset(fs_files[i], 0, sizeof(File));                        // 清空文件内容
            memcpy((char *) fs_files[i]->name, name, strlen(name) + 1);  // 复制文件名
            fs_files[i]->size           = 0;  // 文件大小初始化为 0
            fs_files[i]->current_offset = 0;  // 初始化文件偏移量
            fs_files[i]->nlink          = 1;  // 链接计数初始化为 1

            // 创建第一个数据块
            fs_files[i]->content.content_offset = 0;     // 假设填充一个魔数，用于标识
            fs_files[i]->content.next           = NULL;  // 目前还没有其他数据块
            fs_files[i]->content.prev           = NULL;
            uint64_t addr                       = pmm_alloc_pages_fs(&g_pmm, 1);
            fs_files[i]->content.addr_offset    = (void *) (addr - (uint64_t) fs_head);

            return i;  // 返回文件描述符
        }
    }

    return -1;  // 文件系统已满，无法创建新文件
}

// 关闭文件
int32_t
ramfs_close(int32_t fd)
{
    // 检查文件描述符是否合法
    if (fd < 0 || fd >= MAX_FILES || fs_files[fd]->name[0] == 0) {
        return -1;  // 无效的文件描述符
    }

    // 关闭操作（在内存中不需要实际的资源释放）
    fs_files[fd]->current_offset = 0;  // 重置偏移量，确保下次打开时从头开始
    // 如果有其他状态需要重置，可以在这里进行

    return 0;  // 成功关闭文件
}

// 读取文件
size_t
ramfs_read(int32_t fd, void *buf, size_t count)
{
    if (fd < 0 || fd >= MAX_FILES || fs_files[fd]->name[0] == 0) {
        return -1;  // 无效的文件描述符
    }

    File   *file          = fs_files[fd];
    size_t  bytes_to_read = count;
    size_t  bytes_read    = 0;
    ssize_t offset        = file->current_offset;
    size_t  newoffset     = offset;

    // 从 content 链表中逐块读取数据
    Content *content = &file->content;
    while (content != NULL && offset >= BLOCK_DATA_SIZE) {
        offset -= BLOCK_DATA_SIZE;
        content = content->next;
    }

    while (content != NULL && bytes_read < bytes_to_read) {
        ssize_t block_offset    = offset < BLOCK_DATA_SIZE ? offset : 0;
        size_t  block_remaining = BLOCK_DATA_SIZE - block_offset;
        size_t  to_copy         = (bytes_to_read - bytes_read) < block_remaining
                                      ? (bytes_to_read - bytes_read)
                                      : block_remaining;

        // 复制数据（跳过 Content 结构）
        memcpy(buf + bytes_read,
               (void *) fs_head + (uint64_t) content->addr_offset + sizeof(Content) + block_offset,
               to_copy);

        bytes_read += to_copy;
        newoffset += to_copy;
        offset += to_copy;

        // 如果读取完当前块的内容，跳到下一个块
        if (bytes_read < bytes_to_read) {
            content = content->next;
        }
    }

    // 更新文件的 current_offset
    file->current_offset = newoffset;
    return bytes_read;
}

// 写入文件
size_t
ramfs_write(int32_t fd, const void *buf, size_t count)
{
    if (fd < 0 || fd >= MAX_FILES || fs_files[fd]->name[0] == 0) {
        return -1;  // 无效的文件描述符
    }

    File    *file           = fs_files[fd];
    size_t   bytes_to_write = count;
    size_t   bytes_written  = 0;
    ssize_t  offset         = file->current_offset;  // 文件内总偏移
    uint32_t new_offset     = offset;                // 新偏移只可能比当前偏移大

    // 定位到文件的写入位置
    Content *content = &file->content;
    while (content != NULL && offset >= BLOCK_DATA_SIZE) {
        offset -= BLOCK_DATA_SIZE;
        content = content->next;
    }

    // 从 content 链表中逐块写入数据
    while (content != NULL && bytes_written < bytes_to_write) {
        ssize_t content_offset = offset > 0 ? offset : 0;  // 跳过了一些块，这是当前块要写的位置
        size_t block_remaining = BLOCK_DATA_SIZE - content_offset;
        size_t to_copy         = (bytes_to_write - bytes_written) < block_remaining
                                     ? (bytes_to_write - bytes_written)
                                     : block_remaining;

        uint64_t addr = (uint64_t) fs_head + (uint64_t) content->addr_offset + sizeof(Content);
        // 复制数据到当前块（跳过 Content 结构）
        memcpy((void *) addr + content_offset, buf + bytes_written, to_copy);

        bytes_written += to_copy;
        new_offset += to_copy;
        offset -= to_copy;

        // 如果写完当前块，跳到下一个块
        if (bytes_written < bytes_to_write) {
            if (content->next == NULL) {
                // 创建新的数据块
                Content *new_content =
                    (Content *) pmm_alloc_pages_fs(&g_pmm, 1);  // 使用 PMM 申请一页内存
                if (new_content == NULL) {
                    return -1;  // 内存分配失败
                }
                memset(new_content, 0, sizeof(Content));  // 清空新的 content 结构
                content->next     = new_content;
                new_content->prev = content;
                content           = new_content;

                // 新块的数据区域
                content->addr_offset = (void *) ((uint64_t) new_content - (uint64_t) fs_head);
            } else {
                content = content->next;
            }
        }
    }

    // 更新文件的 current_offset
    file->current_offset = new_offset;
    if (file->current_offset > file->size) {
        file->size = file->current_offset;  // 更新文件的大小
    }

    return bytes_written;
}

// 定位文件指针
off_t
ramfs_lseek(int32_t fd, off_t offset, int32_t whence)
{
    if (fd < 0 || fd >= MAX_FILES || fs_files[fd]->name[0] == 0) {
        return -1;  // 无效的文件描述符
    }

    File *file       = fs_files[fd];
    off_t new_offset = file->current_offset;

    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset += offset;
            break;
        case SEEK_END:  // 这个是相对于末尾来说
            new_offset = file->size + offset;
            break;
        default:
            return -1;  // 无效的 whence 参数
    }

    // 确保新偏移量有效，不越界
    if (new_offset < 0 || new_offset > file->size) {
        return -1;  // 无效的偏移量
    }

    // 更新文件的 current_offset
    file->current_offset = new_offset;
    return new_offset;
}

// 文件控制操作
int32_t
ramfs_fcntl(int32_t fd, int32_t cmd, ...)
{
    // 检查文件描述符是否有效
    if (fd < 0 || fd >= MAX_FILES || fs_files[fd]->name[0] == 0) {
        return -1;  // 无效的文件描述符
    }

    va_list args;
    va_start(args, cmd);

    switch (cmd) {
        case F_GETFL:
            // 返回文件的打开标志（这里假设文件都以 O_RDWR 打开）
            // 如果文件支持不同的打开模式，可以根据实际情况修改返回值
            return O_RDWR;  // 默认返回可读写模式

        case F_SETFL: {
            // 设置文件状态标志
            int32_t flags = va_arg(args, int32_t);  // 获取传入的标志

            // 处理文件的附加操作标志
            if (flags & O_APPEND) {
                // 如果设置了 O_APPEND，文件操作应从文件尾部开始
                // 这里你可以加入逻辑来处理 append 模式
            }
            break;
        }

        default:
            // 未知命令，返回 -1
            return -1;
    }

    va_end(args);
    return 0;  // 成功
}

// 创建硬链接
int32_t
ramfs_link(const char *oldname, const char *newname)
{
    // 查找原文件
    int32_t old_fd = -1;
    for (int32_t i = 0; i < MAX_FILES; i++) {
        if (strcmp(fs_files[i]->name, oldname) == 0) {
            old_fd = i;
            break;
        }
    }

    if (old_fd == -1) {
        return -1;  // 未找到原文件
    }

    // 检查新文件名是否已经存在
    for (int32_t i = 0; i < MAX_FILES; i++) {
        if (strcmp(fs_files[i]->name, newname) == 0) {
            return -1;  // 新文件名已存在
        }
    }

    // 添加新文件
    for (int32_t i = 0; i < MAX_FILES; i++) {
        // 找到一个空槽位，初始化新文件
        if (fs_files[i]->name[0] == 0) {  // 文件名为空，表示该位置尚未使用

            File *new_file = fs_files[i];
            // 使用原文件的文件名、内容和其他属性
            memcpy(new_file->name, newname, sizeof(new_file->name) - 1);
            new_file->name[sizeof(new_file->name) - 1] = '\0';  // 确保字符串结尾
            new_file->size                             = fs_files[old_fd]->size;
            new_file->content = fs_files[old_fd]->content;  // 新文件指向相同的数据区域
            new_file->current_offset = 0;
            new_file->nlink          = 1;  // 新硬链接的链接计数
            fs_files[old_fd]->nlink++;     // 增加原文件的链接计数

            return i;  // 返回新文件的描述符
        }
    }

    return -1;  // 文件系统已满，无法创建硬链接
}

// 删除文件
int32_t
ramfs_unlink(const char *name)
{
    // 查找文件是否存在
    for (int32_t i = 0; i < MAX_FILES; i++) {
        if (strcmp((const char *) fs_files[i]->name, name) == 0) {
            File *file = fs_files[i];

            // 如果文件的链接计数大于 1，则只减少链接计数
            if (file->nlink > 1) {
                file->nlink--;
            } else {
                // 如果文件的链接计数为 1，需要删除文件的内容

                // 遍历文件的 Content 链表，释放每个数据块
                Content *content = &file->content;
                while (content != NULL) {
                    // 计算当前数据块的实际内存地址
                    void *data_block =
                        (void *) ((uint64_t) fs_head + (uint64_t) content->addr_offset);
                    kfree_pages(data_block, 1);  // 释放数据块

                    content = content->next;  // 继续处理下一个数据块
                }

                memset(file, 0, sizeof(File));
            }

            return 0;  // 文件删除成功
        }
    }

    return -1;  // 文件不存在
}

// 重命名文件
int32_t
ramfs_rename(const char *oldname, const char *newname)
{
    // 查找旧文件名
    for (int32_t i = 0; i < MAX_FILES; i++) {
        // 找到匹配的旧文件名
        if (strcmp((const char *) fs_files[i]->name, oldname) == 0) {
            // 检查新文件名是否已经存在
            for (int32_t j = 0; j < MAX_FILES; j++) {
                if (strcmp((const char *) fs_files[j]->name, newname) == 0) {
                    return -1;  // 新文件名已存在
                }
            }

            // 修改文件名
            memcpy((char *) fs_files[i]->name, newname, sizeof(fs_files[i]->name) - 1);
            fs_files[i]->name[sizeof(fs_files[i]->name) - 1] = '\0';  // 确保字符串以 '\0' 结尾

            return 0;  // 重命名成功
        }
    }

    return -1;  // 找不到旧文件名
}

/*
// 获取文件信息
int32_t ramfs_stat(const char *path, struct stat *st) {
    // 查找文件是否存在
    for (int32_t i = 0; i < MAX_FILES; i++) {
        if (fs_files[i]->name[0] != 0 && strcmp((const char*)fs_files[i]->name, path) == 0) {
            // 填充 stat 结构体
            st->st_size = fs_files[i]->size;      // 文件大小
            st->st_nlink = fs_files[i]->nlink;    // 文件链接数
            // st->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 设置文件类型和权限（假设为常规文件，所有者可读写，其他用户可读）
            st->st_uid = 0;  // 假设 UID 为 0
            st->st_gid = 0;  // 假设 GID 为 0
            return 0;  // 找到文件，返回成功
        }
    }

    return -1;  // 未找到文件，返回失败
}

// 获取文件状态
int32_t ramfs_fstat(int32_t fd, struct stat *st) {
    // 检查文件描述符是否合法
    if (fd < 0 || fd >= MAX_FILES || fs_files[fd]->name[0] == 0) {
        return -1;  // 无效的文件描述符
    }

    // 获取文件信息
    File *file = fs_files[fd];
    st->st_size = file->size;       // 文件大小
    st->st_nlink = file->nlink;     // 文件链接计数
    // st->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 假设为常规文件，所有者可读写，其他用户可读
    st->st_uid = 0;                 // 假设文件的用户 ID 为 0
    st->st_gid = 0;                 // 假设文件的组 ID 为 0
    return 0;  // 返回成功
}
*/

/*
DIR* opendir(const char *name) {
    int32_t fd = open(name, O_RDONLY);  // 打开目录文件，O_RDONLY 是只读模式
    if (fd < 0) {
        return NULL;  // 打开失败
    }

    DIR *dir = malloc(sizeof(DIR));
    if (!dir) {
        close(fd);  // 内存分配失败，关闭文件描述符
        return NULL;
    }

    dir->fd = fd;  // 保存文件描述符
    return dir;
}

struct dirent* readdir(DIR *dir) {
    struct dirent *entry = malloc(sizeof(struct dirent));
    if (!entry) {
        return NULL;
    }

    ssize_t bytes_read = read(dir->fd, entry, sizeof(struct dirent));  // 读取目录项
    if (bytes_read <= 0) {
        free(entry);
        return NULL;  // 读取失败或目录末尾
    }

    return entry;
}

int32_t closedir(DIR *dir) {
    int32_t result = close(dir->fd);  // 关闭目录文件描述符
    free(dir);  // 释放目录结构
    return result;
}
*/

uint8_t test_buf[70 * 1024];

// dump binary memory dumpfile2.bin 0x50000000 0x54000000
// restore dumpfile binary 0x50000000

void
basic_test()
{
    int32_t fd = ramfs_open("/home/ajax/1.txt");
    logger("fd: %d\n", fd);

    int32_t size = (uint64_t) __add_bin_end - (uint64_t) __add_bin_start;
    int32_t w    = ramfs_write(fd, __add_bin_start, size);
    logger("write %d bytes\n", w);

    int32_t seek = ramfs_lseek(fd, 0, SEEK_SET);
    logger("seek set 0: %d\n", seek);

    int32_t r = ramfs_read(fd, test_buf, size);
    logger("read %d\n", r);

    if (memcmp(test_buf, __add_bin_start, size) == 0) {
        logger("write read ok!\n");
    }
}

void
ramfs_test()
{
    ramfs_init();
    basic_test();
}