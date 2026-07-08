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
    uint32_t phys_blk;
    uint32_t log_blk;
} FileInfo;

static WINDOW *win_main, *win_status, *win_input, *win_info, *win_fd;
static FileInfo file_list[MAX_FILES];
static int file_count = 0;
static int selected = 0;
static char current_dir_path[MAX_PATH];
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_pos = 0;
static int show_login = 1;

static void gui_init(void);
static void gui_cleanup(void);
static void gui_redraw(void);
static void gui_update_file_list(void);
static void gui_show_info(void);
static void gui_show_fd(void);
static void gui_exec_command(const char *cmd);
static int gui_login(void);

void gui_run(void)
{
    int ch;
    
    gui_init();
    
    if (gui_login()) {
        gui_update_file_list();
        
        while (1) {
            gui_redraw();
            
            ch = wgetch(win_input);
            
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
                    }
                }
            } else if (ch == KEY_UP) {
                if (selected > 0) selected--;
            } else if (ch == KEY_DOWN) {
                if (selected < file_count - 1) selected++;
            } else if (ch == KEY_LEFT || ch == KEY_BACKSPACE || ch == 127) {
                if (cmd_pos > 0) {
                    cmd_pos--;
                    cmd_buffer[cmd_pos] = '\0';
                }
            } else if (ch == KEY_RIGHT) {
                /* ignore */
            } else if (ch == 27) {
                break;
            } else if (ch >= 32 && ch <= 126 && cmd_pos < MAX_CMD_LEN - 1) {
                cmd_buffer[cmd_pos++] = (char)ch;
            }
        }
    }
    
    gui_cleanup();
}

static int gui_login(void)
{
    char username[MAX_USERNAME] = {0};
    char password[MAX_USERNAME] = {0};
    int pos = 0;
    int ch;
    int state = 0;
    
    while (1) {
        wclear(win_main);
        box(win_main, 0, 0);
        mvwprintw(win_main, 5, 10, "Login to File System");
        
        if (state == 0) {
            mvwprintw(win_main, 7, 10, "Username: %s", username);
            mvwprintw(win_main, 9, 10, "Password: ");
        } else if (state == 1) {
            mvwprintw(win_main, 7, 10, "Username: %s", username);
            mvwprintw(win_main, 9, 10, "Password: ******");
        }
        
        wrefresh(win_main);
        
        ch = wgetch(win_main);
        
        if (state == 0) {
            if (ch == '\n') {
                if (pos > 0) {
                    username[pos] = '\0';
                    state = 1;
                    pos = 0;
                }
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (pos > 0) pos--;
            } else if (ch >= 32 && ch < 127 && pos < MAX_USERNAME - 1) {
                username[pos++] = ch;
            }
        } else {
            if (ch == '\n') {
                password[pos] = '\0';
                if (user_login(username, password) == 0) {
                    mvwprintw(win_main, 11, 10, "Login successful!");
                    wrefresh(win_main);
                    napms(500);
                    return 1;
                } else {
                    mvwprintw(win_main, 11, 10, "Login failed! Try again.");
                    wrefresh(win_main);
                    napms(500);
                    state = 0;
                    pos = 0;
                    memset(username, 0, sizeof(username));
                    memset(password, 0, sizeof(password));
                }
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (pos > 0) pos--;
            } else if (ch >= 32 && ch < 127 && pos < MAX_USERNAME - 1) {
                password[pos++] = ch;
            }
        }
    }
    
    return 0;
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
    
    win_main = newwin(rows - 6, cols, 0, 0);
    win_fd = newwin(3, cols, rows - 6, 0);
    win_status = newwin(1, cols, rows - 3, 0);
    win_input = newwin(1, cols, rows - 1, 0);
    win_info = newwin(2, cols, rows - 5, 0);
    
    keypad(win_main, TRUE);
    keypad(win_input, TRUE);
    scrollok(win_main, TRUE);
    
    box(win_main, 0, 0);
    box(win_info, 0, 0);
    box(win_fd, 0, 0);
    
    mvwprintw(win_status, 0, 0, "Linux FS - Arrow keys to select | Enter to confirm | ESC to exit");
}

static void gui_cleanup(void)
{
    delwin(win_main);
    delwin(win_fd);
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
    
    mvwprintw(win_main, 2, 4, "%-24s %-8s %-8s %-10s %-8s %-8s %s", 
              "Name", "Inode", "Size", "Perm", "PhysBlk", "LogBlk", "Type");
    mvwprintw(win_main, 3, 4, "%s", "-------------------------------------------------------------------------------");
    
    for (int i = 0; i < file_count; i++) {
        if (i == selected) {
            wattron(win_main, A_REVERSE);
        }
        mvwprintw(win_main, i + 4, 4, "%-24s %-8u %-8u %-10s %-8u %-8u %s",
                   file_list[i].name,
                   file_list[i].inode,
                   file_list[i].size,
                   file_list[i].perm,
                   file_list[i].phys_blk,
                   file_list[i].log_blk,
                   file_list[i].type);
        if (i == selected) {
            wattroff(win_main, A_REVERSE);
        }
    }
    
    wclear(win_info);
    box(win_info, 0, 0);
    gui_show_info();
    
    wclear(win_fd);
    box(win_fd, 0, 0);
    gui_show_fd();
    
    wclear(win_input);
    mvwprintw(win_input, 0, 0, "> %s", cmd_buffer);
    
    wrefresh(win_main);
    wrefresh(win_fd);
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
        
        if (de_buf.inode == 0 || de_buf.inode >= TOTAL_INODES) {
            offset += de_buf.rec_len;
            continue;
        }
        
        memset(name, 0, sizeof(name));
        memcpy(name, buf + off + sizeof(struct dir_entry),
               de_buf.name_len < MAX_FILENAME ? de_buf.name_len : MAX_FILENAME);
        
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            offset += de_buf.rec_len;
            continue;
        }
        
        read_inode(de_buf.inode, &entry_inode);
        
        strncpy(file_list[file_count].name, name, MAX_FILENAME - 1);
        file_list[file_count].name[MAX_FILENAME - 1] = '\0';
        file_list[file_count].inode = de_buf.inode;
        file_list[file_count].size = entry_inode.i_size;
        perm_to_str(entry_inode.i_mode & 0x1FF, file_list[file_count].perm);
        strncpy(file_list[file_count].type, 
               de_buf.file_type == FT_DIRECTORY ? "<DIR>" : (de_buf.file_type == FT_SYMLINK) ? "<LINK>" : "<FILE>", 
               sizeof(file_list[file_count].type) - 1);
        file_list[file_count].type[sizeof(file_list[file_count].type) - 1] = '\0';
        
        file_list[file_count].log_blk = 0;
        file_list[file_count].phys_blk = 0;
        if (entry_inode.i_size > 0) {
            file_list[file_count].phys_blk = get_block_by_offset(de_buf.inode, 0, 0);
        }
        
        file_count++;
        offset += de_buf.rec_len;
        if (de_buf.rec_len == 0) break;
    }
    
    strncpy(current_dir_path, current_path, MAX_PATH - 1);
    
    if (selected >= file_count) selected = 0;
}

static void gui_show_info(void)
{
    sb_read();
    mvwprintw(win_info, 0, 2, "User: %s | Used: %u | Free: %u | Inode: %u/%u",
              current_uid >= 0 ? current_user : "Not logged in",
              sb.s_blocks_count - sb.s_free_blocks_count,
              sb.s_free_blocks_count,
              sb.s_inodes_count - sb.s_free_inodes_count,
              sb.s_inodes_count);
    
    if (file_count > 0 && selected < file_count) {
        mvwprintw(win_info, 1, 2, "Selected: %s (%s, inode=%u, %u bytes)",
                  file_list[selected].name,
                  file_list[selected].type,
                  file_list[selected].inode,
                  file_list[selected].size);
    }
}

static void gui_show_fd(void)
{
    int i, count = 0;
    mvwprintw(win_fd, 0, 2, "Open FDs: ");
    
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (fd_table[i].used) {
            const char *mode = fd_table[i].mode == OPEN_READ ? "r" :
                              (fd_table[i].mode == OPEN_WRITE ? "w" : "rw");
            if (count == 0) {
                mvwprintw(win_fd, 0, 10, "fd=%d(inode=%u,mode=%s)", i, fd_table[i].inode_no, mode);
            } else {
                wprintw(win_fd, " fd=%d(inode=%u,mode=%s)", i, fd_table[i].inode_no, mode);
            }
            count++;
        }
    }
    
    if (count == 0) {
        mvwprintw(win_fd, 0, 10, "(none)");
    }
    
    mvwprintw(win_fd, 1, 2, "Use: open <file> r/w/rw | read/write <fd> <data> | close <fd>");
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
            char *path = args[1];
            char *sep = strrchr(path, '/');
            if (sep != NULL) {
                *sep = '\0';
                char *parent_path = path;
                char *child_name = sep + 1;
                
                uint32_t parent_ino = dir_lookup(current_dir, parent_path);
                if (parent_ino != 0) {
                    dir_create(child_name, parent_ino);
                } else {
                    printf("父目录 '%s' 不存在\n", parent_path);
                }
                *sep = '/';
            } else {
                dir_create(path, current_dir);
            }
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
    } else if (strcmp(args[0], "ln") == 0) {
        if (strcmp(args[1], "-s") == 0) {
            if (argc >= 4) {
                dir_symlink(args[2], args[3]);
            }
        } else {
            if (argc >= 3) {
                dir_link(args[1], args[2]);
            }
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
        gui_update_file_list();
    } else if (strcmp(args[0], "close") == 0) {
        if (argc >= 2) {
            file_close(atoi(args[1]));
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "read") == 0) {
        if (argc >= 3) {
            int fd = atoi(args[1]);
            int size = atoi(args[2]);
            char *buf = (char *)malloc(size + 1);
            if (buf) {
                int n = file_read(fd, buf, size);
                if (n > 0) {
                    buf[n] = '\0';
                    def_prog_mode();
                    endwin();
                    printf("\nFile content (%d bytes):\n", n);
                    printf("%s\n", buf);
                    printf("Press any key to continue...");
                    getchar();
                    reset_prog_mode();
                    refresh();
                    gui_update_file_list();
                } else if (n == 0) {
                    def_prog_mode();
                    endwin();
                    printf("\n(EOF - 文件末尾)\n");
                    printf("Press any key to continue...");
                    getchar();
                    reset_prog_mode();
                    refresh();
                    gui_update_file_list();
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
        gui_update_file_list();
    } else if (strcmp(args[0], "mmap") == 0) {
        if (argc >= 3) {
            int fd = atoi(args[1]);
            size_t length = (size_t)strtol(args[2], NULL, 10);
            void *addr = fs_mmap(fd, length);
            if (addr != NULL) {
                def_prog_mode();
                endwin();
                printf("\nmmap成功: 地址=%p, 长度=%zu\n", addr, length);
                printf("Press any key to continue...");
                getchar();
                reset_prog_mode();
                refresh();
            }
        }
        gui_update_file_list();
    } else if (strcmp(args[0], "munmap") == 0) {
        if (argc >= 3) {
            void *addr = (void *)strtoul(args[1], NULL, 16);
            size_t length = (size_t)strtol(args[2], NULL, 10);
            fs_munmap(addr, length);
            gui_update_file_list();
        }
    } else if (strcmp(args[0], "msync") == 0) {
        if (argc >= 2) {
            void *addr = (void *)strtoul(args[1], NULL, 16);
            fs_msync(addr);
            gui_update_file_list();
        }
    } else if (strcmp(args[0], "mlist") == 0) {
        def_prog_mode();
        endwin();
        mmap_list();
        printf("\nPress any key to continue...");
        getchar();
        reset_prog_mode();
        refresh();
        gui_update_file_list();
    } else if (strcmp(args[0], "trash") == 0) {
        def_prog_mode();
        endwin();
        trash_list();
        printf("\nPress any key to continue...");
        getchar();
        reset_prog_mode();
        refresh();
        gui_update_file_list();
    } else if (strcmp(args[0], "trash-empty") == 0) {
        trash_empty();
        gui_update_file_list();
    } else if (strcmp(args[0], "journal") == 0) {
        def_prog_mode();
        endwin();
        journal_list();
        printf("\nPress any key to continue...");
        getchar();
        reset_prog_mode();
        refresh();
        gui_update_file_list();
    } else if (strcmp(args[0], "df") == 0) {
        def_prog_mode();
        endwin();
        sb_read();
        printf("Disk usage:\n");
        printf("  Total blocks:    %u\n", sb.s_blocks_count);
        printf("  Used blocks:     %u\n", sb.s_blocks_count - sb.s_free_blocks_count);
        printf("  Free blocks:     %u\n", sb.s_free_blocks_count);
        printf("  Total inodes:    %u\n", sb.s_inodes_count);
        printf("  Used inodes:     %u\n", sb.s_inodes_count - sb.s_free_inodes_count);
        printf("  Free inodes:     %u\n", sb.s_free_inodes_count);
        printf("\nPress any key to continue...");
        getchar();
        reset_prog_mode();
        refresh();
        gui_update_file_list();
    } else if (strcmp(args[0], "help") == 0) {
        def_prog_mode();
        endwin();
        printf("======== Linux Secondary File System - Help ========\n");
        printf("  cd      <dir>           - Change directory\n");
        printf("  create  <file> [perm]   - Create file\n");
        printf("  mkdir   <dir>           - Create directory\n");
        printf("  delete  <file>          - Delete file\n");
        printf("  rmdir   <dir>           - Remove empty directory\n");
        printf("  ln      <target> <link> - Create hard link\n");
        printf("  ln -s   <target> <link> - Create symbolic link\n");
        printf("  open    <file> <mode>   - Open file (r/w/rw)\n");
        printf("  close   <fd>            - Close file\n");
        printf("  read    <fd> <bytes>    - Read file\n");
        printf("  write   <fd> <content>  - Write file\n");
        printf("  chmod   <file> <perm>   - Change permissions\n");
        printf("  trash                   - Show trash\n");
        printf("  trash-move <file>       - Move to trash\n");
        printf("  trash-restore <file>    - Restore from trash\n");
        printf("  trash-empty             - Empty trash\n");
        printf("  journal                 - Show journal\n");
        printf("  df                      - Disk info\n");
        printf("  help                    - Show help\n");
        printf("===================================================\n");
        printf("Press any key to continue...");
        getchar();
        reset_prog_mode();
        refresh();
        gui_update_file_list();
    }
}