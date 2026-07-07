#include "fs.h"
#include <ncurses.h>
#include <ctype.h>

#define MAX_FILES 100
#define MAX_CMD_LEN 256
#define MAX_ARGS 8

typedef struct {
    char name[MAX_FILENAME];
    uint32_t inode;
    uint32_t size;
    char perm[10];
    char type[10];
} FileInfo;

static WINDOW *win_main, *win_status, *win_input, *win_info;
static FileInfo file_list[MAX_FILES];
static int file_count = 0;
static int selected = 0;
static char current_dir_path[MAX_PATH];
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_pos = 0;

static void gui_init(void);
static void gui_cleanup(void);
static void gui_redraw(void);
static void gui_update_file_list(void);
static void gui_show_info(void);
static void gui_exec_command(const char *cmd);

void gui_run(void)
{
    int ch;
    
    gui_init();
    gui_update_file_list();
    
    while (1) {
        gui_redraw();
        
        ch = wgetch(win_main);
        
        if (ch == KEY_F(10) || ch == '\n') {
            if (cmd_pos > 0) {
                cmd_buffer[cmd_pos] = '\0';
                gui_exec_command(cmd_buffer);
                cmd_pos = 0;
                memset(cmd_buffer, 0, sizeof(cmd_buffer));
            } else if (file_count > 0) {
                if (strcmp(file_list[selected].type, "<DIR>") == 0) {
                    char cd_cmd[MAX_PATH];
                    snprintf(cd_cmd, sizeof(cd_cmd), "cd %s", file_list[selected].name);
                    gui_exec_command(cd_cmd);
                } else {
                    char open_cmd[MAX_PATH];
                    snprintf(open_cmd, sizeof(open_cmd), "open %s r", file_list[selected].name);
                    gui_exec_command(open_cmd);
                    gui_exec_command("read 0 1000");
                    gui_exec_command("close 0");
                }
            }
        } else if (ch == KEY_UP) {
            if (selected > 0) selected--;
        } else if (ch == KEY_DOWN) {
            if (selected < file_count - 1) selected++;
        } else if (ch == KEY_LEFT || ch == KEY_BACKSPACE || ch == 127) {
            if (cmd_pos > 0) cmd_pos--;
        } else if (ch == KEY_RIGHT) {
            /* ignore */
        } else if (ch == 27) {
            break;
        } else if (ch >= 32 && ch <= 126 && cmd_pos < MAX_CMD_LEN - 1) {
            cmd_buffer[cmd_pos++] = (char)ch;
        }
    }
    
    gui_cleanup();
}

static void gui_init(void)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    win_main = newwin(rows - 4, cols, 0, 0);
    win_status = newwin(1, cols, rows - 3, 0);
    win_input = newwin(1, cols, rows - 1, 0);
    win_info = newwin(2, cols, rows - 5, 0);
    
    keypad(win_main, TRUE);
    scrollok(win_main, TRUE);
    
    box(win_main, 0, 0);
    box(win_info, 0, 0);
    
    mvwprintw(win_status, 0, 0, "Linux二级文件系统 - 上下键选择 | 回车确认 | ESC退出");
}

static void gui_cleanup(void)
{
    delwin(win_main);
    delwin(win_status);
    delwin(win_input);
    delwin(win_info);
    endwin();
}

static void gui_redraw(void)
{
    wclear(win_main);
    box(win_main, 0, 0);
    
    mvwprintw(win_main, 0, 2, " %s ", current_dir_path);
    
    mvwprintw(win_main, 2, 4, "%-24s %-8s %-8s %-10s %s", 
              "文件名", "Inode", "大小", "权限", "类型");
    mvwprintw(win_main, 3, 4, "%s", "---------------------------------------------------------------");
    
    for (int i = 0; i < file_count; i++) {
        if (i == selected) {
            wattron(win_main, A_REVERSE);
        }
        mvwprintw(win_main, i + 4, 4, "%-24s %-8u %-8u %-10s %s",
                   file_list[i].name,
                   file_list[i].inode,
                   file_list[i].size,
                   file_list[i].perm,
                   file_list[i].type);
        if (i == selected) {
            wattroff(win_main, A_REVERSE);
        }
    }
    
    wclear(win_info);
    box(win_info, 0, 0);
    gui_show_info();
    
    wclear(win_input);
    mvwprintw(win_input, 0, 0, "> %s", cmd_buffer);
    
    wrefresh(win_main);
    wrefresh(win_info);
    wrefresh(win_input);
    wrefresh(win_status);
}

static void gui_update_file_list(void)
{
    struct inode dip;
    uint32_t offset = 0;
    file_count = 0;
    
    read_inode(current_dir, &dip);
    
    while (offset < dip.i_size && file_count < MAX_FILES) {
        struct dir_entry de_buf;
        char name[MAX_FILENAME + 1];
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys;
        char buf[BLOCK_SIZE];
        struct inode entry_inode;
        
        phys = get_block_by_offset(current_dir, blk * BLOCK_SIZE, 0);
        if (phys == 0) break;
        read_block(phys, buf);
        memcpy(&de_buf, buf + off, sizeof(struct dir_entry));
        
        if (de_buf.inode == 0) {
            offset += de_buf.rec_len;
            continue;
        }
        
        memset(name, 0, sizeof(name));
        memcpy(name, buf + off + sizeof(struct dir_entry),
               de_buf.name_len < MAX_FILENAME ? de_buf.name_len : MAX_FILENAME);
        
        read_inode(de_buf.inode, &entry_inode);
        
        strncpy(file_list[file_count].name, name, MAX_FILENAME - 1);
        file_list[file_count].inode = de_buf.inode;
        file_list[file_count].size = entry_inode.i_size;
        perm_to_str(entry_inode.i_mode & 0x1FF, file_list[file_count].perm);
        strcpy(file_list[file_count].type, 
               de_buf.file_type == FT_DIRECTORY ? "<DIR>" : "<FILE>");
        
        file_count++;
        offset += de_buf.rec_len;
        if (de_buf.rec_len == 0) break;
    }
    
    getcwd(current_dir_path, MAX_PATH);
    strncpy(current_dir_path, current_path, MAX_PATH - 1);
    
    if (selected >= file_count) selected = 0;
}

static void gui_show_info(void)
{
    sb_read();
    mvwprintw(win_info, 0, 2, "用户: %s | 已用块: %u | 空闲块: %u | Inode: %u/%u",
              current_uid >= 0 ? current_user : "未登录",
              sb.s_blocks_count - sb.s_free_blocks_count,
              sb.s_free_blocks_count,
              sb.s_inodes_count - sb.s_free_inodes_count,
              sb.s_inodes_count);
    
    if (file_count > 0 && selected < file_count) {
        mvwprintw(win_info, 1, 2, "选中: %s (%s, inode=%u, %u字节)",
                  file_list[selected].name,
                  file_list[selected].type,
                  file_list[selected].inode,
                  file_list[selected].size);
    }
}

static void gui_exec_command(const char *cmd)
{
    char *args[MAX_ARGS];
    int argc = 0;
    char *token, *saveptr;
    char cmd_copy[MAX_CMD_LEN];
    
    strncpy(cmd_copy, cmd, MAX_CMD_LEN - 1);
    cmd_copy[MAX_CMD_LEN - 1] = '\0';
    
    token = strtok_r(cmd_copy, " \t", &saveptr);
    while (token != NULL && argc < MAX_ARGS) {
        args[argc++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    
    if (argc == 0) return;
    
    if (strcmp(args[0], "cd") == 0) {
        if (argc >= 2) {
            dir_change(args[1]);
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "create") == 0) {
        if (argc >= 2) {
            uint16_t perm = DEFAULT_FILE_PERM;
            if (argc >= 3) perm = (uint16_t)strtol(args[2], NULL, 8);
            file_create(args[1], current_dir, perm);
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "mkdir") == 0) {
        if (argc >= 2) {
            dir_create(args[1], current_dir);
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "delete") == 0 || strcmp(args[0], "rm") == 0) {
        if (argc >= 2) {
            file_delete(args[1], current_dir);
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "rmdir") == 0) {
        if (argc >= 2) {
            dir_remove(args[1], current_dir);
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "trash-move") == 0) {
        if (argc >= 2) {
            trash_move(args[1]);
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "trash-restore") == 0) {
        if (argc >= 2) {
            trash_restore(args[1]);
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "chmod") == 0) {
        if (argc >= 3) {
            uint16_t perm = (uint16_t)strtol(args[2], NULL, 8);
            file_chmod(args[1], current_dir, perm);
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "open") == 0) {
        if (argc >= 3) {
            uint16_t mode;
            if (strcmp(args[2], "r") == 0) mode = OPEN_READ;
            else if (strcmp(args[2], "w") == 0) mode = OPEN_WRITE;
            else if (strcmp(args[2], "rw") == 0) mode = OPEN_RDWR;
            else mode = OPEN_READ;
            file_open(args[1], mode);
        }
    } else if (strcmp(args[0], "close") == 0) {
        if (argc >= 2) {
            file_close(atoi(args[1]));
        }
    } else if (strcmp(args[0], "read") == 0) {
        if (argc >= 3) {
            int fd = atoi(args[1]);
            int size = atoi(args[2]);
            char *buf = (char *)malloc(size + 1);
            if (buf) {
                int n = file_read(fd, buf, size);
                if (n > 0) {
                    buf[n] = '\0';
                    gui_cleanup();
                    printf("\n文件内容 (%d 字节):\n", n);
                    printf("%s\n", buf);
                    printf("按任意键继续...");
                    getchar();
                    gui_init();
                }
                free(buf);
            }
        }
    } else if (strcmp(args[0], "write") == 0) {
        if (argc >= 3) {
            int fd = atoi(args[1]);
            char content[MAX_CMD_LEN] = "";
            for (int i = 2; i < argc; i++) {
                if (i > 2) strcat(content, " ");
                strcat(content, args[i]);
            }
            file_write(fd, content, (int)strlen(content));
        }
    } else if (strcmp(args[0], "trash") == 0) {
        gui_cleanup();
        trash_list();
        printf("\n按任意键继续...");
        getchar();
        gui_init();
    } else if (strcmp(args[0], "trash-empty") == 0) {
        trash_empty();
        gui_update_file_list();
    } else if (strcmp(args[0], "journal") == 0) {
        gui_cleanup();
        journal_list();
        printf("\n按任意键继续...");
        getchar();
        gui_init();
    } else if (strcmp(args[0], "df") == 0) {
        gui_cleanup();
        sb_read();
        printf("磁盘使用情况:\n");
        printf("  总块数:     %u\n", sb.s_blocks_count);
        printf("  已用块数:   %u\n", sb.s_blocks_count - sb.s_free_blocks_count);
        printf("  空闲块数:   %u\n", sb.s_free_blocks_count);
        printf("  总inode数:  %u\n", sb.s_inodes_count);
        printf("  已用inode数:%u\n", sb.s_inodes_count - sb.s_free_inodes_count);
        printf("  空闲inode数:%u\n", sb.s_free_inodes_count);
        printf("\n按任意键继续...");
        getchar();
        gui_init();
    } else if (strcmp(args[0], "help") == 0) {
        gui_cleanup();
        printf("======== Linux 二级文件系统 - 命令帮助 ========\n");
        printf("  cd      <目录名>        - 切换目录\n");
        printf("  create  <文件名> [权限] - 创建文件\n");
        printf("  mkdir   <目录名>        - 创建目录\n");
        printf("  delete  <文件名>        - 删除文件\n");
        printf("  rmdir   <目录名>        - 删除空目录\n");
        printf("  open    <文件名> <模式>  - 打开文件\n");
        printf("  close   <fd>            - 关闭文件\n");
        printf("  read    <fd> <字节数>   - 读取文件\n");
        printf("  write   <fd> <内容>     - 写入文件\n");
        printf("  chmod   <文件名> <权限> - 修改权限\n");
        printf("  trash                   - 查看回收站\n");
        printf("  trash-move <文件>       - 移入回收站\n");
        printf("  trash-restore <文件>    - 恢复文件\n");
        printf("  trash-empty             - 清空回收站\n");
        printf("  journal                 - 查看日志\n");
        printf("  df                      - 磁盘信息\n");
        printf("  help                    - 显示帮助\n");
        printf("==============================================\n");
        printf("按任意键继续...");
        getchar();
        gui_init();
    }
}