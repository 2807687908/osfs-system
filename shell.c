#include "fs.h"

#define MAX_ARGS 8
#define MAX_LINE 512

static void show_help(void)
{
    printf("\n");
    printf("======== Linux 二级文件系统 - 命令帮助 ========\n");
    printf("  login  <用户名>         - 用户登录\n");
    printf("  logout                  - 登出当前用户\n");
    printf("  register <用户名>       - 注册新用户\n");
    printf("  dir                     - 列出当前目录内容\n");
    printf("  create  <文件名> [权限] - 创建文件 (默认644)\n");
    printf("  mkdir   <目录名>        - 创建目录\n");
    printf("  delete  <文件名>        - 删除文件\n");
    printf("  rmdir   <目录名>        - 删除空目录\n");
    printf("  open    <文件名> <模式>  - 打开文件 (r/w/rw)\n");
    printf("  close   <fd>            - 关闭文件描述符\n");
    printf("  read    <fd> <字节数>   - 从文件读取\n");
    printf("  write   <fd> <内容>     - 向文件写入\n");
    printf("  chmod   <文件名> <权限> - 修改权限 (例: chmod f 755)\n");
    printf("  cd      <目录名>        - 切换目录\n");
    printf("  pwd                     - 显示当前路径\n");
    printf("  lsof                    - 列出所有打开的文件\n");
    printf("  df                      - 显示磁盘使用情况\n");
    printf("  help                    - 显示此帮助\n");
    printf("  exit                    - 退出系统\n");
    printf("================================================\n\n");
}

/*
 * 显示磁盘使用统计
 */
static void show_df(void)
{
    sb_read();  /* 刷新 */
    printf("磁盘使用情况:\n");
    printf("  总块数:     %u\n", sb.s_blocks_count);
    printf("  已用块数:   %u\n", sb.s_blocks_count - sb.s_free_blocks_count);
    printf("  空闲块数:   %u\n", sb.s_free_blocks_count);
    printf("  总inode数:  %u\n", sb.s_inodes_count);
    printf("  已用inode数:%u\n", sb.s_inodes_count - sb.s_free_inodes_count);
    printf("  空闲inode数:%u\n", sb.s_free_inodes_count);
    printf("  磁盘大小:   %u MB\n", sb.s_blocks_count / 1024);
    printf("  空闲空间:   %u KB\n", sb.s_free_blocks_count);
}

/*
 * 列出打开的文件描述符
 */
static void show_lsof(void)
{
    int i, count = 0;
    printf("打开的文件描述符:\n");
    printf("  fd     inode    offset    mode\n");
    printf("  ------ -------- --------- ----\n");
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (fd_table[i].used) {
            printf("  %-6d %-8u %-9u %s\n",
                   i,
                   fd_table[i].inode_no,
                   fd_table[i].offset,
                   fd_table[i].mode == OPEN_READ ? "r" :
                   (fd_table[i].mode == OPEN_WRITE ? "w" : "rw"));
            count++;
        }
    }
    if (count == 0) {
        printf("  (无打开的文件)\n");
    }
    printf("共 %d 个\n", count);
}

/*
 * 解析命令并执行
 */
static int execute_command(char *line)
{
    char *args[MAX_ARGS];
    int argc = 0;
    char *token, *saveptr;

    /* 去除末尾换行符 */
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
    }

    /* 解析参数 */
    token = strtok_r(line, " \t", &saveptr);
    while (token != NULL && argc < MAX_ARGS) {
        args[argc++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }

    if (argc == 0) return 0;

    /* ===== login ===== */
    if (strcmp(args[0], "login") == 0) {
        if (argc < 2) {
            printf("用法: login <用户名>\n");
            return 0;
        }
        if (current_uid >= 0) {
            printf("请先使用 logout 登出当前用户\n");
            return 0;
        }
        user_login_full(args[1]);
    }
    /* ===== logout ===== */
    else if (strcmp(args[0], "logout") == 0) {
        user_logout();
    }
    /* ===== register ===== */
    else if (strcmp(args[0], "register") == 0) {
        if (argc < 2) {
            printf("用法: register <用户名>\n");
            return 0;
        }
        {
            char passwd[MAX_USERNAME], passwd2[MAX_USERNAME];
            printf("密码: ");
            {
                /* 简单密码输入(回显) */
                fgets(passwd, MAX_USERNAME, stdin);
                passwd[strcspn(passwd, "\r\n")] = '\0';
            }
            printf("确认密码: ");
            {
                fgets(passwd2, MAX_USERNAME, stdin);
                passwd2[strcspn(passwd2, "\r\n")] = '\0';
            }
            if (strcmp(passwd, passwd2) != 0) {
                printf("两次密码不一致\n");
            } else {
                user_register(args[1], passwd);
            }
        }
    }
    /* ===== dir ===== */
    else if (strcmp(args[0], "dir") == 0 || strcmp(args[0], "ls") == 0) {
        if (argc >= 2) {
            uint32_t target;
            if (path_resolve(args[1], &target) == 0) {
                dir_list(target);
            } else {
                printf("路径不存在: %s\n", args[1]);
            }
        } else {
            dir_list(current_dir);
        }
    }
    /* ===== create ===== */
    else if (strcmp(args[0], "create") == 0 || strcmp(args[0], "touch") == 0) {
        if (argc < 2) {
            printf("用法: create <文件名> [权限]\n");
            return 0;
        }
        {
            uint16_t perm = DEFAULT_FILE_PERM;
            if (argc >= 3) {
                perm = (uint16_t)strtol(args[2], NULL, 8);
            }
            file_create(args[1], current_dir, perm);
        }
    }
    /* ===== mkdir ===== */
    else if (strcmp(args[0], "mkdir") == 0) {
        if (argc < 2) {
            printf("用法: mkdir <目录名>\n");
            return 0;
        }
        dir_create(args[1], current_dir);
    }
    /* ===== delete ===== */
    else if (strcmp(args[0], "delete") == 0 || strcmp(args[0], "rm") == 0) {
        if (argc < 2) {
            printf("用法: delete <文件名>\n");
            return 0;
        }
        file_delete(args[1], current_dir);
    }
    /* ===== rmdir ===== */
    else if (strcmp(args[0], "rmdir") == 0) {
        if (argc < 2) {
            printf("用法: rmdir <目录名>\n");
            return 0;
        }
        dir_remove(args[1], current_dir);
    }
    /* ===== open ===== */
    else if (strcmp(args[0], "open") == 0) {
        if (argc < 3) {
            printf("用法: open <文件名> <模式(r/w/rw)>\n");
            return 0;
        }
        {
            uint16_t mode;
            if (strcmp(args[2], "r") == 0)      mode = OPEN_READ;
            else if (strcmp(args[2], "w") == 0) mode = OPEN_WRITE;
            else if (strcmp(args[2], "rw") == 0) mode = OPEN_RDWR;
            else if (strcmp(args[2], "wr") == 0) mode = OPEN_RDWR;
            else {
                printf("无效模式。使用: r (读), w (写), rw (读写)\n");
                return 0;
            }
            file_open(args[1], mode);
        }
    }
    /* ===== close ===== */
    else if (strcmp(args[0], "close") == 0) {
        if (argc < 2) {
            printf("用法: close <fd>\n");
            return 0;
        }
        file_close(atoi(args[1]));
    }
    /* ===== read ===== */
    else if (strcmp(args[0], "read") == 0) {
        if (argc < 3) {
            printf("用法: read <fd> <字节数>\n");
            return 0;
        }
        {
            int fd = atoi(args[1]);
            int size = atoi(args[2]);
            char *buf = (char *)malloc(size + 1);
            if (buf == NULL) {
                printf("内存不足\n");
                return 0;
            }
            int n = file_read(fd, buf, size);
            if (n > 0) {
                buf[n] = '\0';
                printf("读取 %d 字节:\n", n);
                printf("%.*s\n", n, buf);
            } else if (n == 0) {
                printf("(EOF - 文件末尾)\n");
            }
            free(buf);
        }
    }
    /* ===== write ===== */
    else if (strcmp(args[0], "write") == 0) {
        if (argc < 3) {
            printf("用法: write <fd> <内容>\n");
            return 0;
        }
        {
            int fd = atoi(args[1]);
            /* 重新拼接后续参数作为写入内容 */
            char content[MAX_LINE] = "";
            int i;
            for (i = 2; i < argc; i++) {
                if (i > 2) strcat(content, " ");
                strcat(content, args[i]);
            }
            int n = file_write(fd, content, (int)strlen(content));
            if (n >= 0) {
                printf("写入 %d 字节\n", n);
            }
        }
    }
    /* ===== chmod ===== */
    else if (strcmp(args[0], "chmod") == 0) {
        if (argc < 3) {
            printf("用法: chmod <文件名> <八进制权限>\n");
            return 0;
        }
        {
            uint16_t perm = (uint16_t)strtol(args[2], NULL, 8);
            file_chmod(args[1], current_dir, perm);
        }
    }
    /* ===== cd ===== */
    else if (strcmp(args[0], "cd") == 0) {
        if (argc < 2) {
            printf("用法: cd <目录名>\n");
            return 0;
        }
        dir_change(args[1]);
    }
    /* ===== pwd ===== */
    else if (strcmp(args[0], "pwd") == 0) {
        printf("%s\n", current_path);
    }
    /* ===== df ===== */
    else if (strcmp(args[0], "df") == 0) {
        show_df();
    }
    /* ===== lsof ===== */
    else if (strcmp(args[0], "lsof") == 0) {
        show_lsof();
    }
    /* ===== help ===== */
    else if (strcmp(args[0], "help") == 0 || strcmp(args[0], "?") == 0) {
        show_help();
    }
    /* ===== exit ===== */
    else if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0) {
        return 1;  /* 退出信号 */
    }
    else {
        printf("未知命令: '%s'。输入 help 查看帮助。\n", args[0]);
    }

    return 0;
}

/*
 * 交互式主循环
 */
void shell_run(void)
{
    char line[MAX_LINE];

    printf("\n");
    printf("+==========================================+\n");
    printf("|   Linux 二级文件系统 (EXT2模拟) v1.0    |\n");
    printf("|   输入 help 查看命令列表                 |\n");
    printf("+==========================================+\n");
    printf("\n");

    while (1) {
        /* 提示符 */
        if (current_uid >= 0) {
            printf("%s@fs:%s$ ",
                   current_user, current_path);
        } else {
            printf("[未登录]:%s$ ", current_path);
        }
        fflush(stdout);

        if (fgets(line, MAX_LINE, stdin) == NULL) {
            printf("\n");
            break;
        }

        /* 跳过空行 */
        if (line[0] == '\n' || line[0] == '\r') continue;

        if (execute_command(line) != 0) {
            break;
        }
    }
}
