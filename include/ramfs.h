
#ifndef RAM_FS
#define RAM_FS

#include <aj_types.h>

#define MAX_FILES  256

#define BLOCK_SIZE 4096

typedef struct {
    uint64_t magic;
    uint64_t file_total_count;
    uint64_t block_size;
    uint64_t fs_total_size;
} Head;

typedef struct _Content {
    uint64_t content_offset;  // 这个块中数据写到哪个地方了
    struct _Content * next;
    struct _Content * prev;
    void * addr_offset;    // 相对于 Head 头的偏移
} Content;

typedef struct {
    uint8_t  name[512];       // 文件名
    uint32_t size;            // 文件大小
    Content  content;         // 文件内容 每一个block头部放一个 struct content
    uint32_t current_offset;  // 当前文件偏移量
    uint32_t nlink;           // 链接计数
} File;


/* feak define */
#define SEEK_SET 1
#define SEEK_CUR 2
#define SEEK_END 3

#define F_GETFL 6
#define O_RDWR 7
#define F_SETFL 8
#define  O_APPEND 9
/* feak define */

void ramfs_init() ;
void ramfs_test() ;

int ramfs_open(const char *name) ;
int ramfs_close(int fd) ;
size_t ramfs_read(int fd, void *buf, size_t count);
size_t ramfs_write(int fd, const void *buf, size_t count) ;
off_t ramfs_lseek(int fd, off_t offset, int whence);
int ramfs_fcntl(int fd, int cmd, ...) ;
int ramfs_link(const char *oldname, const char *newname);
int ramfs_unlink(const char *name);
int ramfs_rename(const char *oldname, const char *newname);

#endif // RAM_FS