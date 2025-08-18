

#include "io.h"
#include "fs/fat32.h"
#include "fs/fat32_dir.h"
#include "lib/avatar_string.h"

/* ============================================================================
 * 简单的Shell实现
 * ============================================================================ */

#define SHELL_BUFFER_SIZE 256
#define MAX_ARGS 16
#define MAX_PATH_LEN 256

static char shell_buffer[SHELL_BUFFER_SIZE];
static char current_dir[MAX_PATH_LEN] = "/";  // 当前工作目录

// 读取一行输入（使用阻塞方式）
static int shell_read_line(char *buffer, int max_len)
{
    int pos = 0;
    char c;

    while (pos < max_len - 1) {
        // 使用阻塞方式读取字符
        c = getc();  // 阻塞读取一个字符

        if (c == '\r' || c == '\n') {
            // 回车结束输入
            putc('\n');
            buffer[pos] = '\0';
            return pos;
        } else if (c == '\b' || c == 127) {
            // 退格键
            if (pos > 0) {
                pos--;
                putc('\b');
                putc(' ');
                putc('\b');
            }
        } else if (c >= 32 && c <= 126) {
            // 可打印字符
            buffer[pos++] = c;
            putc(c);
        }
        // 忽略其他不可打印字符
    }

    buffer[pos] = '\0';
    return pos;
}

// 解析命令行参数
static int shell_parse_args(char *line, char **args, int max_args)
{
    int argc = 0;
    char *token = line;

    while (*token && argc < max_args - 1) {
        // 跳过空格
        while (*token == ' ' || *token == '\t') {
            token++;
        }

        if (*token == '\0') {
            break;
        }

        args[argc++] = token;

        // 找到下一个空格或结束
        while (*token && *token != ' ' && *token != '\t') {
            token++;
        }

        if (*token) {
            *token++ = '\0';
        }
    }

    args[argc] = NULL;
    return argc;
}

// 路径处理辅助函数
static void normalize_path(char *path)
{
    // 简化路径处理：移除末尾的斜杠（除了根目录）
    int len = strlen(path);
    if (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}

static void resolve_path(const char *input_path, char *resolved_path)
{
    if (input_path[0] == '/') {
        // 绝对路径
        strcpy(resolved_path, input_path);
    } else {
        // 相对路径
        if (strcmp(current_dir, "/") == 0) {
            my_snprintf(resolved_path, MAX_PATH_LEN, "/%s", input_path);
        } else {
            my_snprintf(resolved_path, MAX_PATH_LEN, "%s/%s", current_dir, input_path);
        }
    }
    normalize_path(resolved_path);
}

static int is_valid_directory(const char *path)
{
    // 根目录特殊处理
    if (strcmp(path, "/") == 0) {
        return 1;  // 根目录总是有效的
    }

    fat32_dir_entry_t entry;
    fat32_error_t result = fat32_stat(path, &entry);

    if (result != FAT32_OK) {
        return 0;  // 路径不存在
    }

    return (entry.attr & FAT32_ATTR_DIRECTORY) ? 1 : 0;
}

// ls命令实现
static void shell_cmd_ls(int argc, char **args)
{
    char target_path[MAX_PATH_LEN];

    if (argc > 1) {
        resolve_path(args[1], target_path);
    } else {
        strcpy(target_path, current_dir);
    }

    logger("Listing directory: %s\n", target_path);

    fat32_dir_entry_t entries[20];
    uint32_t count;

    fat32_error_t result = fat32_listdir(target_path, entries, 20, &count);
    if (result != FAT32_OK) {
        logger("Error: %s\n", fat32_get_error_string(result));
        return;
    }

    logger("Total %u entries:\n", count);
    for (uint32_t i = 0; i < count; i++) {
        char filename[13];
        fat32_dir_convert_from_short_name(entries[i].name, filename, sizeof(filename));

        if (entries[i].attr & FAT32_ATTR_DIRECTORY) {
            logger("  [DIR]  %s\n", filename);
        } else {
            logger("  [FILE] %s (%u bytes)\n", filename, entries[i].file_size);
        }
    }
}

// mkdir命令实现
static void shell_cmd_mkdir(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: mkdir <directory_name>\n");
        return;
    }

    char target_path[MAX_PATH_LEN];
    resolve_path(args[1], target_path);

    fat32_error_t result = fat32_mkdir(target_path);
    if (result == FAT32_OK) {
        logger("Directory '%s' created successfully\n", target_path);
    } else {
        logger("Error creating directory: %s\n", fat32_get_error_string(result));
    }
}

// cd命令实现
static void shell_cmd_cd(int argc, char **args)
{
    char target_path[MAX_PATH_LEN];

    if (argc < 2) {
        // 没有参数，切换到根目录
        strcpy(target_path, "/");
    } else if (strcmp(args[1], "..") == 0) {
        // 切换到父目录
        if (strcmp(current_dir, "/") == 0) {
            // 已经在根目录
            logger("Already at root directory\n");
            return;
        } else {
            // 找到最后一个斜杠，截断路径
            strcpy(target_path, current_dir);
            char *last_slash = strrchr(target_path, '/');
            if (last_slash != NULL && last_slash != target_path) {
                *last_slash = '\0';
            } else {
                strcpy(target_path, "/");
            }
        }
    } else {
        // 切换到指定目录
        resolve_path(args[1], target_path);
    }

    // 检查目标路径是否存在且为目录
    if (!is_valid_directory(target_path)) {
        logger("cd: %s: No such directory\n", target_path);
        return;
    }

    // 更新当前工作目录
    strcpy(current_dir, target_path);
    logger("Changed directory to: %s\n", current_dir);
}

// pwd命令实现
static void shell_cmd_pwd(int argc, char **args)
{
    logger("%s\n", current_dir);
}

// rmdir命令实现
static void shell_cmd_rmdir(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: rmdir <directory_name>\n");
        return;
    }

    fat32_error_t result = fat32_rmdir(args[1]);
    if (result == FAT32_OK) {
        logger("Directory '%s' removed successfully\n", args[1]);
    } else {
        logger("Error removing directory: %s\n", fat32_get_error_string(result));
    }
}

// touch命令实现（创建空文件）
static void shell_cmd_touch(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: touch <filename>\n");
        return;
    }

    char target_path[MAX_PATH_LEN];
    resolve_path(args[1], target_path);

    int32_t fd = fat32_open(target_path);
    if (fd > 0) {
        fat32_close(fd);
        logger("File '%s' created successfully\n", target_path);
    } else {
        logger("Error creating file '%s'\n", target_path);
    }
}

// rm命令实现
static void shell_cmd_rm(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: rm <filename>\n");
        return;
    }

    int32_t result = fat32_unlink(args[1]);
    if (result == 0) {
        logger("File '%s' removed successfully\n", args[1]);
    } else {
        logger("Error removing file '%s'\n", args[1]);
    }
}

// cat命令实现（显示文件内容）
static void shell_cmd_cat(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: cat <filename>\n");
        return;
    }

    char target_path[MAX_PATH_LEN];
    resolve_path(args[1], target_path);

    int32_t fd = fat32_open(target_path);
    if (fd <= 0) {
        logger("Error: Cannot open file '%s'\n", target_path);
        return;
    }

    char buffer[256];
    ssize_t bytes_read;

    logger("Content of '%s':\n", target_path);
    logger("--- BEGIN ---\n");

    while ((bytes_read = fat32_read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        logger("%s", buffer);
    }

    logger("\n--- END ---\n");
    fat32_close(fd);
}

// echo命令实现（写入文件）
static void shell_cmd_echo(int argc, char **args)
{
    if (argc < 3) {
        logger("Usage: echo <text> > <filename>\n");
        logger("       echo <text>\n");
        return;
    }

    // 简单实现：echo text > filename
    if (argc >= 4 && strcmp(args[2], ">") == 0) {
        int32_t fd = fat32_open(args[3]);
        if (fd <= 0) {
            logger("Error: Cannot create file '%s'\n", args[3]);
            return;
        }

        ssize_t written = fat32_write(fd, args[1], strlen(args[1]));
        if (written > 0) {
            logger("Written %ld bytes to '%s'\n", written, args[3]);
        } else {
            logger("Error writing to file '%s'\n", args[3]);
        }

        fat32_close(fd);
    } else {
        // 只是显示文本
        logger("%s\n", args[1]);
    }
}

// help命令实现
static void shell_cmd_help(int argc, char **args)
{
    logger("Avatar OS Simple Shell Commands:\n");
    logger("  ls [path]           - List directory contents\n");
    logger("  cd [dir]            - Change directory (cd .. for parent)\n");
    logger("  pwd                 - Print working directory\n");
    logger("  mkdir <dir>         - Create directory\n");
    logger("  rmdir <dir>         - Remove empty directory\n");
    logger("  touch <file>        - Create empty file\n");
    logger("  rm <file>           - Remove file\n");
    logger("  cat <file>          - Display file content\n");
    logger("  echo <text> > <file> - Write text to file\n");
    logger("  echo <text>         - Display text\n");
    logger("  fsinfo              - Show filesystem information\n");
    logger("  clear               - Clear screen\n");
    logger("  help                - Show this help\n");
    logger("  exit                - Exit shell\n");
}

// fsinfo命令实现
static void shell_cmd_fsinfo(int argc, char **args)
{
    if (!fat32_is_mounted()) {
        logger("Filesystem not mounted\n");
        return;
    }

    fat32_print_fs_info();
}

// clear命令实现
static void shell_cmd_clear(int argc, char **args)
{
    logger("\033[2J\033[H");  // ANSI清屏命令
}

// 命令表
typedef struct {
    const char *name;
    void (*func)(int argc, char **args);
    const char *desc;
} shell_command_t;

static const shell_command_t shell_commands[] = {
    {"ls",     shell_cmd_ls,     "List directory contents"},
    {"cd",     shell_cmd_cd,     "Change directory"},
    {"pwd",    shell_cmd_pwd,    "Print working directory"},
    {"mkdir",  shell_cmd_mkdir,  "Create directory"},
    {"rmdir",  shell_cmd_rmdir,  "Remove directory"},
    {"touch",  shell_cmd_touch,  "Create empty file"},
    {"rm",     shell_cmd_rm,     "Remove file"},
    {"cat",    shell_cmd_cat,    "Display file content"},
    {"echo",   shell_cmd_echo,   "Echo text or write to file"},
    {"help",   shell_cmd_help,   "Show help"},
    {"fsinfo", shell_cmd_fsinfo, "Show filesystem info"},
    {"clear",  shell_cmd_clear,  "Clear screen"},
    {NULL, NULL, NULL}
};

// 执行命令
static void shell_execute_command(char *line)
{
    if (strlen(line) == 0) {
        return;
    }

    char *args[MAX_ARGS];
    int argc = shell_parse_args(line, args, MAX_ARGS);

    if (argc == 0) {
        return;
    }

    // 检查退出命令
    if (strcmp(args[0], "exit") == 0) {
        logger("Goodbye!\n");
        return;
    }

    // 查找并执行命令
    for (int i = 0; shell_commands[i].name != NULL; i++) {
        if (strcmp(args[0], shell_commands[i].name) == 0) {
            shell_commands[i].func(argc, args);
            return;
        }
    }

    // 未找到命令
    logger("Unknown command: %s\n", args[0]);
    logger("Type 'help' for available commands.\n");
}

// 主shell循环
void avatar_simple_shell(void)
{
    logger("\n");
    logger("========================================\n");
    logger("    Avatar OS Simple Shell v1.0\n");
    logger("========================================\n");
    logger("Using blocking UART I/O (no interrupts)\n");
    logger("Type 'help' for available commands.\n");
    logger("Type 'exit' to quit.\n");
    logger("\n");

    // 检查文件系统状态
    if (!fat32_is_mounted()) {
        logger("Warning: FAT32 filesystem not mounted!\n");
    } else {
        logger("FAT32 filesystem ready.\n");

        // 显示磁盘后端信息
        if (fat32_disk_is_using_virtio()) {
            logger("Storage: VirtIO Block Device\n");
        } else {
            logger("Storage: Memory Simulation\n");
        }
    }

    logger("\n");

    while (1) {
        logger("avatar:%s $ ", current_dir);

        int len = shell_read_line(shell_buffer, SHELL_BUFFER_SIZE);
        if (len > 0) {
            shell_execute_command(shell_buffer);

            // 检查是否是退出命令
            if (strncmp(shell_buffer, "exit", 4) == 0) {
                break;
            }
        }
    }

    logger("Shell exited.\n");
}