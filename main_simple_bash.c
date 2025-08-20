/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file main_simple_bash.c
 * @brief Implementation of main_simple_bash.c
 * @author Avatar Project Team
 * @date 2024
 */


#include "io.h"
#include "fs/fat32.h"
#include "fs/fat32_dir.h"
#include "fs/fat32_utils.h"
#include "lib/avatar_string.h"
#include "guest/guest_manifest.h"
#include "vmm/guest_loader.h"
#include "vmm/vm.h"
#include "vmm/vpl011.h"

/* ============================================================================
 * 简单的Shell实现
 * ============================================================================ */

#define SHELL_BUFFER_SIZE 256
#define MAX_ARGS          16
#define MAX_PATH_LEN      256

static char shell_buffer[SHELL_BUFFER_SIZE];
static char current_dir[MAX_PATH_LEN] = "/";  // 当前工作目录

// 读取一行输入（使用阻塞方式）
static int
shell_read_line(char *buffer, int max_len)
{
    int  pos = 0;
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
static int
shell_parse_args(char *line, char **args, int max_args)
{
    int   argc  = 0;
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
static void
normalize_path(char *path)
{
    // 简化路径处理：移除末尾的斜杠（除了根目录）
    int len = strlen(path);
    if (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}

static void
resolve_path(const char *input_path, char *resolved_path)
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

static int
is_valid_directory(const char *path)
{
    // 根目录特殊处理
    if (strcmp(path, "/") == 0) {
        return 1;  // 根目录总是有效的
    }

    fat32_dir_entry_t entry;
    fat32_error_t     result = fat32_stat(path, &entry);

    if (result != FAT32_OK) {
        return 0;  // 路径不存在
    }

    return (entry.attr & FAT32_ATTR_DIRECTORY) ? 1 : 0;
}

// ls命令实现
static void
shell_cmd_ls(int argc, char **args)
{
    char target_path[MAX_PATH_LEN];

    if (argc > 1) {
        resolve_path(args[1], target_path);
    } else {
        strcpy(target_path, current_dir);
    }

    logger("Listing directory: %s\n", target_path);

    fat32_dir_entry_t entries[20];
    uint32_t          count;

    fat32_error_t result = fat32_listdir(target_path, entries, 20, &count);
    if (result != FAT32_OK) {
        logger("Error: %s\n", fat32_get_error_string(result));
        return;
    }

    logger("Total %u entries:\n", count);
    for (uint32_t i = 0; i < count; i++) {
        char filename[13];
        fat32_dir_convert_from_dir_entry(&entries[i], filename, sizeof(filename));

        if (entries[i].attr & FAT32_ATTR_DIRECTORY) {
            logger_info("%s\n", filename);
        } else {
            logger("%s (%u bytes)\n", filename, entries[i].file_size);
        }
    }
}

// mkdir命令实现
static void
shell_cmd_mkdir(int argc, char **args)
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
static void
shell_cmd_cd(int argc, char **args)
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
static void
shell_cmd_pwd(int argc, char **args)
{
    logger("%s\n", current_dir);
}

// rmdir命令实现
static void
shell_cmd_rmdir(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: rmdir <directory_name>\n");
        return;
    }

    char target_path[MAX_PATH_LEN];
    resolve_path(args[1], target_path);

    fat32_error_t result = fat32_rmdir(target_path);
    if (result == FAT32_OK) {
        logger("Directory '%s' removed successfully\n", target_path);
    } else {
        logger("Error removing directory: %s\n", fat32_get_error_string(result));
    }
}

// touch命令实现（创建空文件）
static void
shell_cmd_touch(int argc, char **args)
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
static void
shell_cmd_rm(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: rm <filename>\n");
        return;
    }

    char target_path[MAX_PATH_LEN];
    resolve_path(args[1], target_path);

    int32_t result = fat32_unlink(target_path);
    if (result == 0) {
        logger("File '%s' removed successfully\n", target_path);
    } else {
        logger("Error removing file '%s'\n", target_path);
    }
}

// cat命令实现（显示文件内容）
static void
shell_cmd_cat(int argc, char **args)
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

    char    buffer[256];
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
static void
shell_cmd_echo(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: echo <text> [> <filename>]\n");
        logger("       echo <text> <filename>  (write to file)\n");
        return;
    }

    // 检查是否是重定向到文件：echo text > filename
    if (argc >= 4 && strcmp(args[2], ">") == 0) {
        char target_path[MAX_PATH_LEN];
        resolve_path(args[3], target_path);

        int32_t fd = fat32_open(target_path);
        if (fd <= 0) {
            logger("Error: Cannot create file '%s'\n", target_path);
            return;
        }

        ssize_t written = fat32_write(fd, args[1], strlen(args[1]));
        if (written > 0) {
            logger("Written %ld bytes to '%s'\n", written, target_path);
        } else {
            logger("Error writing to file '%s'\n", target_path);
        }

        fat32_close(fd);
    } else if (argc == 3) {
        // 简化语法：echo text filename (直接写入文件)
        char target_path[MAX_PATH_LEN];
        resolve_path(args[2], target_path);

        int32_t fd = fat32_open(target_path);
        if (fd <= 0) {
            logger("Error: Cannot create file '%s'\n", target_path);
            return;
        }

        ssize_t written = fat32_write(fd, args[1], strlen(args[1]));
        if (written > 0) {
            logger("Written %ld bytes to '%s'\n", written, target_path);
        } else {
            logger("Error writing to file '%s'\n", target_path);
        }

        fat32_close(fd);
    } else {
        // 只是显示文本
        logger("%s\n", args[1]);
    }
}

// vi编辑器实现
#define VI_MAX_LINES    100
#define VI_MAX_LINE_LEN 256
#define VI_BUFFER_SIZE  (VI_MAX_LINES * VI_MAX_LINE_LEN)

typedef enum
{
    VI_MODE_NORMAL,
    VI_MODE_INSERT,
    VI_MODE_COMMAND
} vi_mode_t;

typedef struct
{
    char      lines[VI_MAX_LINES][VI_MAX_LINE_LEN];
    int       line_count;
    int       cursor_row;
    int       cursor_col;
    vi_mode_t mode;
    char      filename[MAX_PATH_LEN];
    int       modified;
} vi_editor_t;

static vi_editor_t vi_editor;

// 初始化编辑器
static void
vi_init(const char *filename)
{
    memset(&vi_editor, 0, sizeof(vi_editor));
    if (filename) {
        strcpy(vi_editor.filename, filename);
    }
    vi_editor.mode        = VI_MODE_NORMAL;
    vi_editor.line_count  = 1;
    vi_editor.lines[0][0] = '\0';
}

// 显示编辑器状态
static void
vi_display()
{
    logger("\033[2J\033[H");  // 清屏并移动到左上角

    // 显示文件内容
    for (int i = 0; i < vi_editor.line_count && i < 20; i++) {
        if (i == vi_editor.cursor_row) {
            // 显示当前行，用光标标记
            for (int j = 0; j < strlen(vi_editor.lines[i]); j++) {
                if (j == vi_editor.cursor_col) {
                    logger("\033[7m%c\033[0m", vi_editor.lines[i][j] ? vi_editor.lines[i][j] : ' ');
                } else {
                    logger("%c", vi_editor.lines[i][j]);
                }
            }
            if (vi_editor.cursor_col >= strlen(vi_editor.lines[i])) {
                logger("\033[7m \033[0m");  // 显示光标在行末
            }
            logger("\n");
        } else {
            logger("%s\n", vi_editor.lines[i]);
        }
    }

    // 显示状态行
    logger("\033[%d;1H", 22);  // 移动到第22行
    logger("-- ");
    switch (vi_editor.mode) {
        case VI_MODE_NORMAL:
            logger("NORMAL");
            break;
        case VI_MODE_INSERT:
            logger("INSERT");
            break;
        case VI_MODE_COMMAND:
            logger("COMMAND");
            break;
    }
    logger(" -- %s %s",
           vi_editor.filename[0] ? vi_editor.filename : "[No Name]",
           vi_editor.modified ? "[Modified]" : "");

    logger("\033[23;1H");  // 移动到第23行
    logger("Row: %d, Col: %d", vi_editor.cursor_row + 1, vi_editor.cursor_col + 1);
}

// 加载文件
static int
vi_load_file(const char *filename)
{
    char resolved_path[MAX_PATH_LEN];
    resolve_path(filename, resolved_path);

    int32_t fd = fat32_open_readonly(resolved_path);
    if (fd <= 0) {
        // 文件不存在，创建新文件
        vi_editor.line_count  = 1;
        vi_editor.lines[0][0] = '\0';
        return 0;
    }

    char   buffer[VI_BUFFER_SIZE];
    size_t bytes_read = fat32_read(fd, buffer, sizeof(buffer) - 1);
    fat32_close(fd);

    if (bytes_read == 0) {
        vi_editor.line_count  = 1;
        vi_editor.lines[0][0] = '\0';
        return 0;
    }

    buffer[bytes_read] = '\0';

    // 解析文件内容到行数组
    vi_editor.line_count = 0;
    int line_pos         = 0;

    for (size_t i = 0; i < bytes_read && vi_editor.line_count < VI_MAX_LINES; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\r') {
            vi_editor.lines[vi_editor.line_count][line_pos] = '\0';
            vi_editor.line_count++;
            line_pos = 0;

            // 跳过 \r\n 组合中的第二个字符
            if (buffer[i] == '\r' && i + 1 < bytes_read && buffer[i + 1] == '\n') {
                i++;
            }
        } else if (line_pos < VI_MAX_LINE_LEN - 1) {
            vi_editor.lines[vi_editor.line_count][line_pos++] = buffer[i];
        }
    }

    // 处理最后一行（如果没有换行符结尾）
    if (line_pos > 0 || vi_editor.line_count == 0) {
        if (vi_editor.line_count < VI_MAX_LINES) {
            vi_editor.lines[vi_editor.line_count][line_pos] = '\0';
            vi_editor.line_count++;
        }
    }

    if (vi_editor.line_count == 0) {
        vi_editor.line_count  = 1;
        vi_editor.lines[0][0] = '\0';
    }

    return 1;
}

// 保存文件
static int
vi_save_file()
{
    if (vi_editor.filename[0] == '\0') {
        return 0;  // 没有文件名
    }

    char resolved_path[MAX_PATH_LEN];
    resolve_path(vi_editor.filename, resolved_path);

    int32_t fd = fat32_open(resolved_path);
    if (fd <= 0) {
        return 0;
    }

    // 写入所有行
    for (int i = 0; i < vi_editor.line_count; i++) {
        fat32_write(fd, vi_editor.lines[i], strlen(vi_editor.lines[i]));
        if (i < vi_editor.line_count - 1) {
            fat32_write(fd, "\n", 1);
        }
    }

    fat32_close(fd);
    vi_editor.modified = 0;
    return 1;
}

// 插入字符
static void
vi_insert_char(char c)
{
    char *line = vi_editor.lines[vi_editor.cursor_row];
    int   len  = strlen(line);

    if (len < VI_MAX_LINE_LEN - 1) {
        // 向右移动光标后的字符
        for (int i = len; i >= vi_editor.cursor_col; i--) {
            line[i + 1] = line[i];
        }
        line[vi_editor.cursor_col] = c;
        vi_editor.cursor_col++;
        vi_editor.modified = 1;
    }
}

// 删除字符（退格）
static void
vi_delete_char()
{
    if (vi_editor.cursor_col > 0) {
        char *line = vi_editor.lines[vi_editor.cursor_row];
        int   len  = strlen(line);

        // 向左移动光标后的字符
        for (int i = vi_editor.cursor_col - 1; i < len; i++) {
            line[i] = line[i + 1];
        }
        vi_editor.cursor_col--;
        vi_editor.modified = 1;
    } else if (vi_editor.cursor_row > 0) {
        // 合并到上一行
        int prev_len = strlen(vi_editor.lines[vi_editor.cursor_row - 1]);
        if (prev_len + strlen(vi_editor.lines[vi_editor.cursor_row]) < VI_MAX_LINE_LEN - 1) {
            strcat(vi_editor.lines[vi_editor.cursor_row - 1],
                   vi_editor.lines[vi_editor.cursor_row]);

            // 删除当前行
            for (int i = vi_editor.cursor_row; i < vi_editor.line_count - 1; i++) {
                strcpy(vi_editor.lines[i], vi_editor.lines[i + 1]);
            }
            vi_editor.line_count--;
            vi_editor.cursor_row--;
            vi_editor.cursor_col = prev_len;
            vi_editor.modified   = 1;
        }
    }
}

// 插入新行
static void
vi_insert_newline()
{
    if (vi_editor.line_count < VI_MAX_LINES) {
        // 向下移动后续行
        for (int i = vi_editor.line_count; i > vi_editor.cursor_row; i--) {
            strcpy(vi_editor.lines[i], vi_editor.lines[i - 1]);
        }

        // 分割当前行
        char *current_line = vi_editor.lines[vi_editor.cursor_row];
        strcpy(vi_editor.lines[vi_editor.cursor_row + 1], &current_line[vi_editor.cursor_col]);
        current_line[vi_editor.cursor_col] = '\0';

        vi_editor.line_count++;
        vi_editor.cursor_row++;
        vi_editor.cursor_col = 0;
        vi_editor.modified   = 1;
    }
}

// 移动光标
static void
vi_move_cursor(int row_delta, int col_delta)
{
    int new_row = vi_editor.cursor_row + row_delta;
    int new_col = vi_editor.cursor_col + col_delta;

    // 限制行范围
    if (new_row < 0)
        new_row = 0;
    if (new_row >= vi_editor.line_count)
        new_row = vi_editor.line_count - 1;

    // 限制列范围
    int line_len = strlen(vi_editor.lines[new_row]);
    if (new_col < 0)
        new_col = 0;
    if (new_col > line_len)
        new_col = line_len;

    vi_editor.cursor_row = new_row;
    vi_editor.cursor_col = new_col;
}

// 处理普通模式按键
static int
vi_handle_normal_mode(char c)
{
    switch (c) {
        case 'h':  // 左移
            vi_move_cursor(0, -1);
            break;
        case 'j':  // 下移
            vi_move_cursor(1, 0);
            break;
        case 'k':  // 上移
            vi_move_cursor(-1, 0);
            break;
        case 'l':  // 右移
            vi_move_cursor(0, 1);
            break;
        case 'i':  // 进入插入模式
            vi_editor.mode = VI_MODE_INSERT;
            break;
        case 'a':  // 在光标后插入
            vi_move_cursor(0, 1);
            vi_editor.mode = VI_MODE_INSERT;
            break;
        case 'o':  // 在下一行插入
            vi_editor.cursor_col = strlen(vi_editor.lines[vi_editor.cursor_row]);
            vi_insert_newline();
            vi_editor.mode = VI_MODE_INSERT;
            break;
        case 'x':  // 删除当前字符
            if (vi_editor.cursor_col < strlen(vi_editor.lines[vi_editor.cursor_row])) {
                vi_move_cursor(0, 1);
                vi_delete_char();
            }
            break;
        case ':':  // 进入命令模式
            vi_editor.mode = VI_MODE_COMMAND;
            break;
        case 'q':  // 退出（简化版）
            return 0;
        default:
            break;
    }
    return 1;
}

// 处理插入模式按键
static int
vi_handle_insert_mode(char c)
{
    switch (c) {
        case 27:  // ESC键，返回普通模式
            vi_editor.mode = VI_MODE_NORMAL;
            if (vi_editor.cursor_col > 0) {
                vi_editor.cursor_col--;
            }
            break;
        case '\r':
        case '\n':  // 回车键
            vi_insert_newline();
            break;
        case '\b':
        case 127:  // 退格键
            vi_delete_char();
            break;
        default:
            if (c >= 32 && c <= 126) {  // 可打印字符
                vi_insert_char(c);
            }
            break;
    }
    return 1;
}

// 处理命令模式
static int
vi_handle_command_mode()
{
    logger("\033[24;1H:");  // 移动到最后一行显示命令提示符

    char cmd_buffer[64];
    int  cmd_pos = 0;
    char c;

    while (1) {
        c = getc();

        if (c == '\r' || c == '\n') {
            cmd_buffer[cmd_pos] = '\0';
            break;
        } else if (c == '\b' || c == 127) {
            if (cmd_pos > 0) {
                cmd_pos--;
                putc('\b');
                putc(' ');
                putc('\b');
            }
        } else if (c == 27) {  // ESC
            vi_editor.mode = VI_MODE_NORMAL;
            return 1;
        } else if (c >= 32 && c <= 126 && cmd_pos < sizeof(cmd_buffer) - 1) {
            cmd_buffer[cmd_pos++] = c;
            putc(c);
        }
    }

    // 处理命令
    if (strcmp(cmd_buffer, "q") == 0) {
        if (vi_editor.modified) {
            logger("\nFile modified. Use :q! to quit without saving, or :wq to save and quit.");
            getc();  // 等待用户按键
        } else {
            return 0;  // 退出
        }
    } else if (strcmp(cmd_buffer, "q!") == 0) {
        return 0;  // 强制退出
    } else if (strcmp(cmd_buffer, "w") == 0) {
        if (vi_save_file()) {
            logger("\nFile saved.");
        } else {
            logger("\nError saving file.");
        }
        getc();  // 等待用户按键
    } else if (strcmp(cmd_buffer, "wq") == 0) {
        if (vi_save_file()) {
            return 0;  // 保存并退出
        } else {
            logger("\nError saving file.");
            getc();  // 等待用户按键
        }
    } else {
        logger("\nUnknown command: %s", cmd_buffer);
        getc();  // 等待用户按键
    }

    vi_editor.mode = VI_MODE_NORMAL;
    return 1;
}

// vi主循环
static void
vi_main_loop()
{
    char c;
    int  running = 1;

    while (running) {
        vi_display();
        c = getc();

        switch (vi_editor.mode) {
            case VI_MODE_NORMAL:
                running = vi_handle_normal_mode(c);
                break;
            case VI_MODE_INSERT:
                running = vi_handle_insert_mode(c);
                break;
            case VI_MODE_COMMAND:
                running = vi_handle_command_mode();
                break;
        }
    }
}

// cp命令实现（文件复制）
static void
shell_cmd_cp(int argc, char **args)
{
    if (argc < 3) {
        logger("Usage: cp <source> <destination>\n");
        return;
    }

    char src_path[MAX_PATH_LEN], dst_path[MAX_PATH_LEN];
    resolve_path(args[1], src_path);
    resolve_path(args[2], dst_path);

    // 检查源文件是否存在
    fat32_dir_entry_t src_info;
    fat32_error_t     result = fat32_stat(src_path, &src_info);
    if (result != FAT32_OK) {
        logger("cp: cannot stat '%s': %s\n", src_path, fat32_get_error_string(result));
        return;
    }

    // 检查源文件是否为目录
    if (src_info.attr & FAT32_ATTR_DIRECTORY) {
        logger("cp: '%s' is a directory (not supported)\n", src_path);
        return;
    }

    // 检查目标文件是否已存在
    fat32_dir_entry_t dst_info;
    result = fat32_stat(dst_path, &dst_info);
    if (result == FAT32_OK) {
        logger("cp: '%s' already exists. Overwrite? (y/n): ", dst_path);
        char c = getc();
        putc(c);
        putc('\n');
        if (c != 'y' && c != 'Y') {
            logger("Copy cancelled.\n");
            return;
        }
        // 删除现有文件
        fat32_unlink(dst_path);
    }

    // 打开源文件
    int32_t src_fd = fat32_open_readonly(src_path);
    if (src_fd <= 0) {
        logger("cp: cannot open '%s' for reading\n", src_path);
        return;
    }

    // 创建目标文件
    int32_t dst_fd = fat32_open(dst_path);
    if (dst_fd <= 0) {
        logger("cp: cannot create '%s'\n", dst_path);
        fat32_close(src_fd);
        return;
    }

    // 复制文件内容
    char   buffer[512];  // 使用512字节缓冲区
    size_t total_copied = 0;
    size_t bytes_read;

    logger("Copying '%s' to '%s'...\n", src_path, dst_path);

    while ((bytes_read = fat32_read(src_fd, buffer, sizeof(buffer))) > 0) {
        size_t bytes_written = fat32_write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            logger("cp: write error to '%s'\n", dst_path);
            fat32_close(src_fd);
            fat32_close(dst_fd);
            fat32_unlink(dst_path);  // 删除不完整的文件
            return;
        }
        total_copied += bytes_written;
    }

    // 关闭文件
    fat32_close(src_fd);
    fat32_close(dst_fd);

    logger("Copied %zu bytes from '%s' to '%s'\n", total_copied, src_path, dst_path);
}

// mv命令实现（文件移动/重命名）
static void
shell_cmd_mv(int argc, char **args)
{
    if (argc < 3) {
        logger("Usage: mv <source> <destination>\n");
        return;
    }

    char src_path[MAX_PATH_LEN], dst_path[MAX_PATH_LEN];
    resolve_path(args[1], src_path);
    resolve_path(args[2], dst_path);

    // 检查源文件是否存在
    fat32_dir_entry_t src_info;
    fat32_error_t     result = fat32_stat(src_path, &src_info);
    if (result != FAT32_OK) {
        logger("mv: cannot stat '%s': %s\n", src_path, fat32_get_error_string(result));
        return;
    }

    // 检查源文件是否为目录
    if (src_info.attr & FAT32_ATTR_DIRECTORY) {
        logger("mv: '%s' is a directory (not supported)\n", src_path);
        return;
    }

    // 检查目标文件是否已存在
    fat32_dir_entry_t dst_info;
    result = fat32_stat(dst_path, &dst_info);
    if (result == FAT32_OK) {
        logger("mv: '%s' already exists. Overwrite? (y/n): ", dst_path);
        char c = getc();
        putc(c);
        putc('\n');
        if (c != 'y' && c != 'Y') {
            logger("Move cancelled.\n");
            return;
        }
    }

    // 尝试使用重命名功能（如果支持的话）
    result = fat32_rename(src_path, dst_path);
    if (result == FAT32_OK) {
        logger("Moved '%s' to '%s'\n", src_path, dst_path);
        return;
    }

    // 重命名不支持，使用复制+删除的方式
    logger("Rename not supported, using copy+delete method...\n");

    // 如果目标文件存在，先删除
    if (fat32_stat(dst_path, &dst_info) == FAT32_OK) {
        fat32_unlink(dst_path);
    }

    // 打开源文件
    int32_t src_fd = fat32_open_readonly(src_path);
    if (src_fd <= 0) {
        logger("mv: cannot open '%s' for reading\n", src_path);
        return;
    }

    // 创建目标文件
    int32_t dst_fd = fat32_open(dst_path);
    if (dst_fd <= 0) {
        logger("mv: cannot create '%s'\n", dst_path);
        fat32_close(src_fd);
        return;
    }

    // 复制文件内容
    char   buffer[512];
    size_t total_copied = 0;
    size_t bytes_read;
    int    copy_success = 1;

    while ((bytes_read = fat32_read(src_fd, buffer, sizeof(buffer))) > 0) {
        size_t bytes_written = fat32_write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            logger("mv: write error to '%s'\n", dst_path);
            copy_success = 0;
            break;
        }
        total_copied += bytes_written;
    }

    // 关闭文件
    fat32_close(src_fd);
    fat32_close(dst_fd);

    if (copy_success) {
        // 复制成功，删除源文件
        if (fat32_unlink(src_path) == 0) {
            logger("Moved %zu bytes from '%s' to '%s'\n", total_copied, src_path, dst_path);
        } else {
            logger("mv: copied file but failed to remove source '%s'\n", src_path);
        }
    } else {
        // 复制失败，删除不完整的目标文件
        fat32_unlink(dst_path);
        logger("mv: failed to move '%s' to '%s'\n", src_path, dst_path);
    }
}

// tree命令实现（树形显示目录结构）
#define TREE_MAX_DEPTH   10
#define TREE_MAX_ENTRIES 50

// 树形显示的符号
#define TREE_BRANCH   "├── "
#define TREE_LAST     "└── "
#define TREE_VERTICAL "│   "
#define TREE_SPACE    "    "

// 递归显示目录树
static void
tree_show_directory(const char *path,
                    const char *prefix,
                    int         is_last,
                    int         depth,
                    int        *total_dirs,
                    int        *total_files)
{
    if (depth >= TREE_MAX_DEPTH) {
        logger("%s%s[depth limit reached]\n", prefix, is_last ? TREE_LAST : TREE_BRANCH);
        return;
    }

    // 获取目录内容
    fat32_dir_entry_t entries[TREE_MAX_ENTRIES];
    uint32_t          count;
    fat32_error_t     result = fat32_listdir(path, entries, TREE_MAX_ENTRIES, &count);

    if (result != FAT32_OK) {
        logger("%s%s[error: %s]\n",
               prefix,
               is_last ? TREE_LAST : TREE_BRANCH,
               fat32_get_error_string(result));
        return;
    }

    // 过滤掉 "." 和 ".." 条目
    int valid_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        char filename[13];
        fat32_dir_convert_from_dir_entry(&entries[i], filename, sizeof(filename));

        // 跳过 "." 和 ".." 条目
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
            continue;
        }

        // 将有效条目移到前面
        if (valid_count != i) {
            entries[valid_count] = entries[i];
        }
        valid_count++;
    }

    // 显示每个条目
    for (int i = 0; i < valid_count; i++) {
        char filename[13];
        fat32_dir_convert_from_dir_entry(&entries[i], filename, sizeof(filename));

        int is_last_entry = (i == valid_count - 1);

        // 显示当前条目
        if (entries[i].attr & FAT32_ATTR_DIRECTORY) {
            // 目录
            logger("%s%s%s/\n", prefix, is_last_entry ? TREE_LAST : TREE_BRANCH, filename);
            (*total_dirs)++;

            // 构建子目录路径
            char subdir_path[MAX_PATH_LEN];
            if (strcmp(path, "/") == 0) {
                my_snprintf(subdir_path, sizeof(subdir_path), "/%s", filename);
            } else {
                my_snprintf(subdir_path, sizeof(subdir_path), "%s/%s", path, filename);
            }

            // 构建新的前缀
            char new_prefix[256];
            my_snprintf(new_prefix,
                        sizeof(new_prefix),
                        "%s%s",
                        prefix,
                        is_last_entry ? TREE_SPACE : TREE_VERTICAL);

            // 递归显示子目录
            tree_show_directory(subdir_path, new_prefix, 1, depth + 1, total_dirs, total_files);
        } else {
            // 文件
            logger("%s%s%s", prefix, is_last_entry ? TREE_LAST : TREE_BRANCH, filename);
            if (entries[i].file_size > 0) {
                logger(" (%u bytes)", entries[i].file_size);
            }
            logger("\n");
            (*total_files)++;
        }
    }
}

// tree命令主函数
static void
shell_cmd_tree(int argc, char **args)
{
    char target_path[MAX_PATH_LEN];

    // 确定要显示的路径
    if (argc > 1) {
        resolve_path(args[1], target_path);
    } else {
        strcpy(target_path, current_dir);
    }

    // 特殊处理根目录
    if (strcmp(target_path, "/") == 0) {
        // 根目录直接显示，不需要 stat 检查
        logger("%s\n", target_path);

        // 统计计数器
        int total_dirs  = 0;
        int total_files = 0;

        // 显示目录树
        tree_show_directory(target_path, "", 1, 0, &total_dirs, &total_files);

        // 显示统计信息
        logger("\n%d directories, %d files\n", total_dirs, total_files);
        return;
    }

    // 检查非根目录路径是否存在且为目录
    fat32_dir_entry_t path_info;
    fat32_error_t     result = fat32_stat(target_path, &path_info);
    if (result != FAT32_OK) {
        logger("tree: cannot access '%s': %s\n", target_path, fat32_get_error_string(result));
        return;
    }

    if (!(path_info.attr & FAT32_ATTR_DIRECTORY)) {
        logger("tree: '%s' is not a directory\n", target_path);
        return;
    }

    // 显示路径
    logger("%s\n", target_path);

    // 统计计数器
    int total_dirs  = 0;
    int total_files = 0;

    // 显示目录树
    tree_show_directory(target_path, "", 1, 0, &total_dirs, &total_files);

    // 显示统计信息
    logger("\n%d directories, %d files\n", total_dirs, total_files);
}

// du命令实现（磁盘使用情况）
#define DU_MAX_DEPTH   20
#define DU_MAX_ENTRIES 100

// 递归计算目录大小
static uint64_t
du_calculate_directory_size(const char *path, int show_subdirs, int human_readable, int depth)
{
    if (depth >= DU_MAX_DEPTH) {
        return 0;  // 防止无限递归
    }

    uint64_t total_size = 0;

    // 获取目录内容
    fat32_dir_entry_t entries[DU_MAX_ENTRIES];
    uint32_t          count;
    fat32_error_t     result = fat32_listdir(path, entries, DU_MAX_ENTRIES, &count);

    if (result != FAT32_OK) {
        if (depth == 0) {  // 只在顶层显示错误
            logger("du: cannot access '%s': %s\n", path, fat32_get_error_string(result));
        }
        return 0;
    }

    // 遍历目录中的每个条目
    for (uint32_t i = 0; i < count; i++) {
        char filename[13];
        fat32_dir_convert_from_dir_entry(&entries[i], filename, sizeof(filename));

        // 跳过 "." 和 ".." 条目
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
            continue;
        }

        if (entries[i].attr & FAT32_ATTR_DIRECTORY) {
            // 子目录
            char subdir_path[MAX_PATH_LEN];
            if (strcmp(path, "/") == 0) {
                my_snprintf(subdir_path, sizeof(subdir_path), "/%s", filename);
            } else {
                my_snprintf(subdir_path, sizeof(subdir_path), "%s/%s", path, filename);
            }

            // 递归计算子目录大小
            uint64_t subdir_size =
                du_calculate_directory_size(subdir_path, show_subdirs, human_readable, depth + 1);
            total_size += subdir_size;

            // 如果需要显示子目录，则显示
            if (show_subdirs) {
                if (human_readable) {
                    char size_str[20];
                    if (fat32_utils_format_file_size((uint32_t) subdir_size,
                                                     size_str,
                                                     sizeof(size_str)) == FAT32_OK) {
                        logger("%s\t%s\n", size_str, subdir_path);
                    } else {
                        logger("%llu\t%s\n", subdir_size, subdir_path);
                    }
                } else {
                    // 以字节为单位显示
                    logger("%llu\t%s\n", subdir_size, subdir_path);
                }
            }
        } else {
            // 文件
            total_size += entries[i].file_size;
        }
    }

    return total_size;
}

// du命令主函数
static void
shell_cmd_du(int argc, char **args)
{
    char target_path[MAX_PATH_LEN];
    int  show_subdirs   = 0;  // -a 选项：显示所有子目录
    int  human_readable = 0;  // -h 选项：人性化显示
    int  summary_only   = 0;  // -s 选项：只显示总计
    int  arg_index      = 1;

    // 解析选项
    while (arg_index < argc && args[arg_index][0] == '-') {
        char *option = args[arg_index];
        for (int i = 1; option[i] != '\0'; i++) {
            switch (option[i]) {
                case 'a':
                    show_subdirs = 1;
                    break;
                case 'h':
                    human_readable = 1;
                    break;
                case 's':
                    summary_only = 1;
                    break;
                default:
                    logger("du: invalid option -- '%c'\n", option[i]);
                    logger("Usage: du [-ahs] [path]\n");
                    logger("  -a  show all subdirectories\n");
                    logger("  -h  human readable format\n");
                    logger("  -s  summary only\n");
                    return;
            }
        }
        arg_index++;
    }

    // 确定要分析的路径
    if (arg_index < argc) {
        resolve_path(args[arg_index], target_path);
    } else {
        strcpy(target_path, current_dir);
    }

    // 特殊处理根目录
    if (strcmp(target_path, "/") == 0) {
        // 根目录直接处理
        uint64_t total_size = du_calculate_directory_size(target_path,
                                                          show_subdirs && !summary_only,
                                                          human_readable,
                                                          0);

        // 显示总计
        if (human_readable) {
            char size_str[20];
            if (fat32_utils_format_file_size((uint32_t) total_size, size_str, sizeof(size_str)) ==
                FAT32_OK) {
                logger("%s\t%s\n", size_str, target_path);
            } else {
                logger("%llu\t%s\n", total_size, target_path);
            }
        } else {
            logger("%llu\t%s\n", total_size, target_path);
        }
        return;
    }

    // 检查路径是否存在
    fat32_dir_entry_t path_info;
    fat32_error_t     result = fat32_stat(target_path, &path_info);
    if (result != FAT32_OK) {
        logger("du: cannot access '%s': %s\n", target_path, fat32_get_error_string(result));
        return;
    }

    if (path_info.attr & FAT32_ATTR_DIRECTORY) {
        // 目录
        uint64_t total_size = du_calculate_directory_size(target_path,
                                                          show_subdirs && !summary_only,
                                                          human_readable,
                                                          0);

        // 显示总计
        if (human_readable) {
            char size_str[20];
            if (fat32_utils_format_file_size((uint32_t) total_size, size_str, sizeof(size_str)) ==
                FAT32_OK) {
                logger("%s\t%s\n", size_str, target_path);
            } else {
                logger("%llu\t%s\n", total_size, target_path);
            }
        } else {
            logger("%llu\t%s\n", total_size, target_path);
        }
    } else {
        // 单个文件
        if (human_readable) {
            char size_str[20];
            if (fat32_utils_format_file_size(path_info.file_size, size_str, sizeof(size_str)) ==
                FAT32_OK) {
                logger("%s\t%s\n", size_str, target_path);
            } else {
                logger("%u\t%s\n", path_info.file_size, target_path);
            }
        } else {
            logger("%u\t%s\n", path_info.file_size, target_path);
        }
    }
}

// vi命令实现
static void
shell_cmd_vi(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: vi <filename>\n");
        return;
    }

    char target_path[MAX_PATH_LEN];
    resolve_path(args[1], target_path);

    // 初始化编辑器
    vi_init(target_path);

    // 加载文件
    vi_load_file(target_path);

    // 显示帮助信息
    logger("Simple Vi Editor\n");
    logger("Normal mode commands:\n");
    logger("  h,j,k,l - Move cursor (left,down,up,right)\n");
    logger("  i - Insert mode\n");
    logger("  a - Append (insert after cursor)\n");
    logger("  o - Open new line below\n");
    logger("  x - Delete character\n");
    logger("  : - Command mode\n");
    logger("  q - Quit (if no changes)\n");
    logger("\nInsert mode:\n");
    logger("  ESC - Return to normal mode\n");
    logger("  Backspace - Delete character\n");
    logger("  Enter - New line\n");
    logger("\nCommand mode:\n");
    logger("  :w - Save file\n");
    logger("  :q - Quit\n");
    logger("  :wq - Save and quit\n");
    logger("  :q! - Quit without saving\n");
    logger("\nPress any key to start...\n");
    getc();

    // 进入主循环
    vi_main_loop();

    // 清屏并返回shell
    logger("\033[2J\033[H");
    logger("Exited vi editor.\n");
}

// help命令实现
static void
shell_cmd_help(int argc, char **args)
{
    logger("Avatar OS Simple Shell Commands:\n");
    logger("  ls [path]           - List directory contents\n");
    logger("  cd [dir]            - Change directory (cd .. for parent)\n");
    logger("  pwd                 - Print working directory\n");
    logger("  mkdir <dir>         - Create directory\n");
    logger("  rmdir <dir>         - Remove empty directory\n");
    logger("  touch <file>        - Create empty file\n");
    logger("  rm <file>           - Remove file\n");
    logger("  cp <src> <dst>      - Copy file\n");
    logger("  mv <src> <dst>      - Move/rename file\n");
    logger("  cat <file>          - Display file content\n");
    logger("  echo <text> > <file> - Write text to file\n");
    logger("  echo <text>         - Display text\n");
    logger("  vi <file>           - Edit file with simple vi editor\n");
    logger("  tree [path]         - Display directory tree structure\n");
    logger("  du [-ahs] [path]    - Display disk usage\n");
    logger("  fsinfo              - Show filesystem information\n");
    logger("  guest <subcmd>      - Guest management commands\n");
    logger("    guest config show - Show console configuration\n");
    logger("    guest config set  - Set console configuration\n");
    logger("  clear               - Clear screen\n");
    logger("  help                - Show this help\n");
    logger("  exit                - Exit shell\n");
}

// fsinfo命令实现
static void
shell_cmd_fsinfo(int argc, char **args)
{
    if (!fat32_is_mounted()) {
        logger("Filesystem not mounted\n");
        return;
    }

    fat32_print_fs_info();
}

// clear命令实现
static void
shell_cmd_clear(int argc, char **args)
{
    logger("\033[2J\033[H");  // ANSI清屏命令
}

// guest list命令实现
static void
shell_cmd_guest_list(int argc, char **args)
{
    logger("Available guests:\n");
    logger("ID | %-15s | %-10s | %-30s | DTB | Initrd\n", "Name", "Type", "Kernel Path");
    logger("---|-----------------|------------|--------------------------------|-----|-------\n");

    for (uint32_t i = 0; i < guest_manifest_count; i++) {
        const guest_manifest_t *manifest = &guest_manifests[i];
        logger("%2u | %-15s | %-10s | %-30s | %-3s | %-6s\n",
               i,
               manifest->name,
               guest_type_to_string(manifest->type),
               manifest->files.kernel_path,
               manifest->files.needs_dtb ? "Yes" : "No",
               manifest->files.needs_initrd ? "Yes" : "No");
    }
    logger("\n");
}

// guest info命令实现
static void
shell_cmd_guest_info(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: guest info <guest_id>\n");
        return;
    }

    uint32_t guest_id = atol(args[1]);
    if (guest_id >= guest_manifest_count) {
        logger("Invalid guest ID: %u (max: %u)\n", guest_id, guest_manifest_count - 1);
        return;
    }

    const guest_manifest_t *manifest = &guest_manifests[guest_id];

    logger("Guest Information:\n");
    logger("  ID:           %u\n", guest_id);
    logger("  Name:         %s\n", manifest->name);
    logger("  Type:         %s\n", guest_type_to_string(manifest->type));
    logger("  vCPUs:        %u\n", manifest->smp_num);
    logger("  Kernel Path:  %s\n", manifest->files.kernel_path);
    logger("  Kernel Addr:  0x%llx\n", manifest->bin_loadaddr);

    if (manifest->files.needs_dtb) {
        logger("  DTB Path:     %s\n", manifest->files.dtb_path);
        logger("  DTB Addr:     0x%llx\n", manifest->dtb_loadaddr);
    }

    if (manifest->files.needs_initrd) {
        logger("  Initrd Path:  %s\n", manifest->files.initrd_path);
        logger("  Initrd Addr:  0x%llx\n", manifest->fs_loadaddr);
    }

    // 检查文件是否存在
    if (fat32_is_mounted()) {
        logger("\nFile Status:\n");
        bool files_valid = guest_validate_files(manifest);
        logger("  Files Valid:  %s\n", files_valid ? "Yes" : "No");

        if (files_valid) {
            size_t kernel_size, dtb_size, initrd_size;
            if (guest_get_file_sizes(manifest, &kernel_size, &dtb_size, &initrd_size)) {
                logger("  Kernel Size:  %zu bytes\n", kernel_size);
                if (manifest->files.needs_dtb && dtb_size > 0) {
                    logger("  DTB Size:     %zu bytes\n", dtb_size);
                }
                if (manifest->files.needs_initrd && initrd_size > 0) {
                    logger("  Initrd Size:  %zu bytes\n", initrd_size);
                }
            }
        }
    } else {
        logger("\nFile Status: Cannot check (filesystem not mounted)\n");
    }
    logger("\n");
}

// guest validate命令实现
static void
shell_cmd_guest_validate(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: guest validate <guest_id>\n");
        return;
    }

    uint32_t guest_id = atol(args[1]);
    if (guest_id >= guest_manifest_count) {
        logger("Invalid guest ID: %u (max: %u)\n", guest_id, guest_manifest_count - 1);
        return;
    }

    const guest_manifest_t *manifest = &guest_manifests[guest_id];

    if (!fat32_is_mounted()) {
        logger("Error: Filesystem not mounted\n");
        return;
    }

    logger("Validating guest: %s\n", manifest->name);

    bool valid = guest_validate_files(manifest);
    if (valid) {
        logger("✓ All required files are present\n");

        size_t kernel_size, dtb_size, initrd_size;
        if (guest_get_file_sizes(manifest, &kernel_size, &dtb_size, &initrd_size)) {
            logger("✓ File sizes obtained successfully\n");
            logger("  Kernel: %zu bytes\n", kernel_size);
            if (manifest->files.needs_dtb) {
                logger("  DTB: %zu bytes\n", dtb_size);
            }
            if (manifest->files.needs_initrd) {
                logger("  Initrd: %zu bytes\n", initrd_size);
            }
        }
    } else {
        logger("✗ Some required files are missing\n");
    }
    logger("\n");
}

// guest start命令实现
static void
shell_cmd_guest_start(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: guest start <guest_id>\n");
        return;
    }

    uint32_t guest_id = atol(args[1]);
    if (guest_id >= guest_manifest_count) {
        logger("Invalid guest ID: %u (max: %u)\n", guest_id, guest_manifest_count - 1);
        return;
    }

    const guest_manifest_t *manifest = &guest_manifests[guest_id];

    logger("Starting guest: %s (ID: %u)\n", manifest->name, guest_id);

    // 验证文件存在
    if (!fat32_is_mounted()) {
        logger("Error: Filesystem not mounted\n");
        return;
    }

    if (!guest_validate_files(manifest)) {
        logger("Error: Guest files validation failed\n");
        return;
    }

    // 分配VM
    struct _vm_t *vm = alloc_vm();
    if (!vm) {
        logger("Error: Failed to allocate VM\n");
        return;
    }

    logger("VM allocated with ID: %u\n", vm->vm_id);

    // 使用新的manifest初始化VM
    vm_init_with_manifest(vm, manifest);

    // 启动VM
    logger("Starting VM...\n");
    run_vm(vm);

    logger("Guest %s started successfully!\n", manifest->name);
}


// guest config命令实现
static void
shell_cmd_guest_config_show(int argc, char **args)
{
    logger("=== Guest Console Configuration ===\n");

    logger("Active VM: %u\n", vpl011_get_current_console_vm());

    logger("Console Switching: %s\n",
           vpl011_get_console_switching_enabled() ? "enabled" : "disabled");

    uint32_t strategy = vpl011_get_output_strategy();
    logger("Output Strategy: %s\n",
           strategy == CONSOLE_OUTPUT_ALL_WITH_PREFIX ? "all_with_prefix" : "active_only");

    logger("====================================\n");
}

static void
shell_cmd_guest_config_set(int argc, char **args)
{
    if (argc < 3) {
        logger("Usage: guest config set <option> <value>\n");
        logger("Options:\n");
        logger("  switching <true/false>     - Enable/disable console switching\n");
        logger("  strategy <all_with_prefix/active_only> - Set output strategy\n");
        logger("  active_vm <id>             - Set active VM\n");
        return;
    }

    const char *option = args[1];
    const char *value  = args[2];

    if (strcmp(option, "switching") == 0) {
        if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            vpl011_set_console_switching(true);
            logger("Console switching enabled\n");
        } else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            vpl011_set_console_switching(false);
            logger("Console switching disabled\n");
        } else {
            logger("Invalid value. Use: true/false or 1/0\n");
        }
    } else if (strcmp(option, "strategy") == 0) {
        if (strcmp(value, "all_with_prefix") == 0 || strcmp(value, "0") == 0) {
            vpl011_set_output_strategy(CONSOLE_OUTPUT_ALL_WITH_PREFIX);
            logger("Output strategy set to: all_with_prefix\n");
        } else if (strcmp(value, "active_only") == 0 || strcmp(value, "1") == 0) {
            vpl011_set_output_strategy(CONSOLE_OUTPUT_ACTIVE_ONLY);
            logger("Output strategy set to: active_only\n");
        } else {
            logger("Invalid strategy. Use: all_with_prefix/active_only or 0/1\n");
        }
    } else if (strcmp(option, "active_vm") == 0) {
        uint32_t vm_id = atol(value);
        vpl011_set_active_vm(vm_id);
        logger("Active VM set to: %u\n", vm_id);
    } else {
        logger("Unknown option: %s\n", option);
        logger("Available options: switching, strategy, active_vm\n");
    }
}

static void
shell_cmd_guest_config(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: guest config <subcommand> [args...]\n");
        logger("Subcommands:\n");
        logger("  show                    - Show current console configuration\n");
        logger("  set <option> <value>    - Set configuration option\n");
        return;
    }

    const char *subcmd = args[1];

    if (strcmp(subcmd, "show") == 0) {
        shell_cmd_guest_config_show(argc - 1, args + 1);
    } else if (strcmp(subcmd, "set") == 0) {
        shell_cmd_guest_config_set(argc - 1, args + 1);
    } else {
        logger("Unknown config subcommand: %s\n", subcmd);
        logger("Type 'guest config' for usage information.\n");
    }
}

// guest主命令实现
static void
shell_cmd_guest(int argc, char **args)
{
    if (argc < 2) {
        logger("Usage: guest <subcommand> [args...]\n");
        logger("Subcommands:\n");
        logger("  list                - List all available guests\n");
        logger("  info <guest_id>     - Show detailed guest information\n");
        logger("  validate <guest_id> - Validate guest files\n");
        logger("  start <guest_id>    - Start a guest VM\n");
        logger("  config <subcmd>     - Console configuration management\n");
        return;
    }

    const char *subcmd = args[1];

    if (strcmp(subcmd, "list") == 0) {
        shell_cmd_guest_list(argc - 1, args + 1);
    } else if (strcmp(subcmd, "info") == 0) {
        shell_cmd_guest_info(argc - 1, args + 1);
    } else if (strcmp(subcmd, "validate") == 0) {
        shell_cmd_guest_validate(argc - 1, args + 1);
    } else if (strcmp(subcmd, "start") == 0) {
        shell_cmd_guest_start(argc - 1, args + 1);
    } else if (strcmp(subcmd, "config") == 0) {
        shell_cmd_guest_config(argc - 1, args + 1);
    } else {
        logger("Unknown guest subcommand: %s\n", subcmd);
        logger("Type 'guest' for usage information.\n");
    }
}

// 命令表
typedef struct
{
    const char *name;
    void (*func)(int argc, char **args);
    const char *desc;
} shell_command_t;

static const shell_command_t shell_commands[] = {
    {"ls", shell_cmd_ls, "List directory contents"},
    {"cd", shell_cmd_cd, "Change directory"},
    {"pwd", shell_cmd_pwd, "Print working directory"},
    {"mkdir", shell_cmd_mkdir, "Create directory"},
    {"rmdir", shell_cmd_rmdir, "Remove directory"},
    {"touch", shell_cmd_touch, "Create empty file"},
    {"rm", shell_cmd_rm, "Remove file"},
    {"cp", shell_cmd_cp, "Copy file"},
    {"mv", shell_cmd_mv, "Move/rename file"},
    {"cat", shell_cmd_cat, "Display file content"},
    {"echo", shell_cmd_echo, "Echo text or write to file"},
    {"vi", shell_cmd_vi, "Edit file with simple vi editor"},
    {"tree", shell_cmd_tree, "Display directory tree structure"},
    {"du", shell_cmd_du, "Display disk usage"},
    {"guest", shell_cmd_guest, "Guest management commands"},
    {"help", shell_cmd_help, "Show help"},
    {"fsinfo", shell_cmd_fsinfo, "Show filesystem info"},
    {"clear", shell_cmd_clear, "Clear screen"},
    {NULL, NULL, NULL}};

// 执行命令
static void
shell_execute_command(char *line)
{
    if (strlen(line) == 0) {
        return;
    }

    char *args[MAX_ARGS];
    int   argc = shell_parse_args(line, args, MAX_ARGS);

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
void
avatar_simple_shell(void)
{
    // 设置VPL011默认配置
    vpl011_set_active_vm(0);                                 // 默认活跃VM为0
    vpl011_set_console_switching(true);                      // 默认启用控制台切换
    vpl011_set_output_strategy(CONSOLE_OUTPUT_ACTIVE_ONLY);  // 默认只输出当前VM的输出

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