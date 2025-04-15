

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_FILES 10
#define BLOCK_SIZE 1024

typedef struct {
    char *name;            // 文件名
    size_t size;           // 文件大小
    void *data;            // 文件内容（指向内存中的数据）
    off_t current_offset;  // 当前文件偏移量
    int nlink;             // 链接计数
} File;

static File ramfs[MAX_FILES];  // 存储文件的数组
static int num_files = 0;      // 当前文件数目

// 打开文件
int ramfs_open(const char *name) {
    for (int i = 0; i < num_files; i++) {
        if (strcmp(ramfs[i].name, name) == 0) {
            ramfs[i].current_offset = 0;  // 重置偏移量
            return i;
        }
    }

    if (num_files < MAX_FILES) {
        ramfs[num_files].name = strdup(name);
        ramfs[num_files].size = 0;
        ramfs[num_files].data = malloc(BLOCK_SIZE);
        if (ramfs[num_files].data == NULL) {
            return -1;
        }
        ramfs[num_files].current_offset = 0;  // 初始化偏移量
        ramfs[num_files].nlink = 1;  // 新文件，初始化链接计数为1
        return num_files++;
    }

    return -1;
}

// 关闭文件
int ramfs_close(int fd) {
    if (fd < 0 || fd >= num_files) {
        return -1;
    }
    // 不需要处理任何关闭操作，因为文件存储在内存中，不涉及资源释放
    return 0;
}

// 读取文件
ssize_t ramfs_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >= num_files) {
        return -1;
    }

    File *file = &ramfs[fd];
    size_t bytes_to_read = count;
    if (file->current_offset + bytes_to_read > file->size) {
        bytes_to_read = file->size - file->current_offset;
    }

    memcpy(buf, file->data + file->current_offset, bytes_to_read);
    file->current_offset += bytes_to_read;

    return bytes_to_read;
}

// 写入文件
ssize_t ramfs_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >= num_files) {
        return -1;
    }

    File *file = &ramfs[fd];
    size_t bytes_to_write = count;
    if (file->current_offset + bytes_to_write > BLOCK_SIZE) {
        bytes_to_write = BLOCK_SIZE - file->current_offset;
    }

    memcpy(file->data + file->current_offset, buf, bytes_to_write);
    file->current_offset += bytes_to_write;
    if (file->current_offset > file->size) {
        file->size = file->current_offset;
    }

    return bytes_to_write;
}

// 定位文件指针
off_t ramfs_lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= num_files) {
        return -1;
    }

    File *file = &ramfs[fd];
    off_t new_offset = file->current_offset;

    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset += offset;
            break;
        case SEEK_END:
            new_offset = file->size + offset;
            break;
        default:
            return -1;
    }

    if (new_offset < 0 || new_offset > file->size) {
        return -1;
    }

    file->current_offset = new_offset;
    return new_offset;
}

// 获取文件信息
int ramfs_stat(const char *path, struct stat *st) {
    for (int i = 0; i < num_files; i++) {
        if (strcmp(ramfs[i].name, path) == 0) {
            st->st_size = ramfs[i].size;
            st->st_nlink = ramfs[i].nlink;
            return 0;
        }
    }

    return -1;
}

// 获取文件状态
int ramfs_fstat(int fd, struct stat *st) {
    if (fd < 0 || fd >= num_files) {
        return -1;
    }

    st->st_size = ramfs[fd].size;
    st->st_nlink = ramfs[fd].nlink;
    return 0;
}

// 文件控制操作
int ramfs_fcntl(int fd, int cmd, ...) {
    if (fd < 0 || fd >= num_files) {
        return -1;
    }

    va_list args;
    va_start(args, cmd);
    switch (cmd) {
        case F_GETFL:
            return O_RDWR;
        case F_SETFL:
            break;
        default:
            return -1;
    }
    va_end(args);

    return 0;
}

// 创建硬链接
int ramfs_link(const char *oldname, const char *newname) {
    for (int i = 0; i < num_files; i++) {
        if (strcmp(ramfs[i].name, oldname) == 0) {
            for (int j = 0; j < num_files; j++) {
                if (strcmp(ramfs[j].name, newname) == 0) {
                    return -1;  // 新文件名已存在
                }
            }

            ramfs[num_files].name = strdup(newname);
            ramfs[num_files].size = ramfs[i].size;
            ramfs[num_files].data = ramfs[i].data;
            ramfs[num_files].current_offset = 0;
            ramfs[num_files].nlink = 1;
            ramfs[i].nlink++;
            return num_files++;
        }
    }

    return -1;
}

// 删除文件
int ramfs_unlink(const char *name) {
    for (int i = 0; i < num_files; i++) {
        if (strcmp(ramfs[i].name, name) == 0) {
            if (ramfs[i].nlink > 1) {
                ramfs[i].nlink--;
            } else {
                free(ramfs[i].name);
                free(ramfs[i].data);
                ramfs[i] = ramfs[num_files - 1];
                num_files--;
            }

            return 0;
        }
    }

    return -1;
}

// 重命名文件
int ramfs_rename(const char *oldname, const char *newname) {
    for (int i = 0; i < num_files; i++) {
        if (strcmp(ramfs[i].name, oldname) == 0) {
            for (int j = 0; j < num_files; j++) {
                if (strcmp(ramfs[j].name, newname) == 0) {
                    return -1;  // 新文件名已存在
                }
            }

            free(ramfs[i].name);
            ramfs[i].name = strdup(newname);
            return 0;
        }
    }

    return -1;
}

