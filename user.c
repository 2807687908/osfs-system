#include "fs.h"

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#define isatty _isatty
#define STDIN_FILENO 0
#else
#include <termios.h>
#include <unistd.h>
static void set_echo(int enable)
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if (enable)
        tty.c_lflag |= ECHO;
    else
        tty.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}
#endif

/*
 * 读取密码(不回显)
 * 非终端时降级为fgets(用于自动化测试)
 */
static void read_password(char *buf, int max_len)
{
    int i = 0, ch;

    /* 非交互式终端: 用fgets读取(如管道输入) */
    if (!isatty(STDIN_FILENO)) {
        if (fgets(buf, max_len, stdin)) {
            size_t len = strlen(buf);
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
                buf[--len] = '\0';
        }
        return;
    }

    /* 交互式终端: 不回显读取 */
#ifdef _WIN32
    while (i < max_len - 1) {
        ch = getch();
        if (ch == '\r' || ch == '\n' || ch == EOF) break;
        if (ch == '\b' || ch == 127) {
            if (i > 0) { i--; printf("\b \b"); }
            continue;
        }
        buf[i++] = (char)ch;
        putchar('*');
    }
#else
    set_echo(0);
    while (i < max_len - 1) {
        ch = getchar();
        if (ch == '\n' || ch == EOF) break;
        if (ch == 127 || ch == '\b') {
            if (i > 0) { i--; printf("\b \b"); }
            continue;
        }
        buf[i++] = (char)ch;
        putchar('*');
    }
    set_echo(1);
#endif
    buf[i] = '\0';
    printf("\n");
}

/*
 * 验证用户名和密码
 * 返回: >=0 成功(uid), -1 密码错误, -2 用户不存在
 */
int user_verify(const char *name, const char *passwd)
{
    struct user_entry users[MAX_USERS];
    uint32_t i;
    user_read_table(users);
    for (i = 0; i < sb.s_user_count && i < MAX_USERS; i++) {
        if (strcmp(users[i].name, name) == 0) {
            if (strcmp(users[i].passwd, passwd) == 0)
                return (int)users[i].uid;
            return -1;  /* 密码错误 */
        }
    }
    return -2;  /* 用户不存在 */
}

/*
 * 登录(交互式，带密码提示)
 */
int user_login_full(const char *name)
{
    char passwd[MAX_USERNAME];
    int uid;

    uid = user_verify(name, "");
    if (uid == -2) {
        printf("用户 '%s' 不存在\n", name);
        return -1;
    }
    printf("密码: ");
    read_password(passwd, MAX_USERNAME);
    uid = user_verify(name, passwd);
    if (uid >= 0) {
        strcpy(current_user, name);
        current_uid = uid;
        printf("登录成功! 欢迎 %s (uid=%d)\n", name, uid);
        return uid;
    }
    printf("密码错误!\n");
    return -1;
}

/*
 * 登录(非交互式)
 */
int user_login(const char *name, const char *passwd)
{
    struct user_entry users[MAX_USERS];
    uint32_t i;
    user_read_table(users);
    for (i = 0; i < sb.s_user_count && i < MAX_USERS; i++) {
        if (strcmp(users[i].name, name) == 0) {
            if (strcmp(users[i].passwd, passwd) == 0) {
                strcpy(current_user, name);
                current_uid = (int)users[i].uid;
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

/*
 * 登出
 */
void user_logout(void)
{
    if (current_uid >= 0) {
        printf("用户 %s 已登出\n", current_user);
        current_uid = -1;
        current_user[0] = '\0';
    }
}

/*
 * 注册新用户
 */
int user_register(const char *name, const char *passwd)
{
    struct user_entry users[MAX_USERS];
    uint32_t i;
    user_read_table(users);

    if (sb.s_user_count >= MAX_USERS) {
        printf("用户数已达上限(%d)\n", MAX_USERS);
        return -1;
    }
    for (i = 0; i < sb.s_user_count; i++) {
        if (strcmp(users[i].name, name) == 0) {
            printf("用户 '%s' 已存在\n", name);
            return -1;
        }
    }

    i = sb.s_user_count;
    strncpy(users[i].name, name, MAX_USERNAME - 1);
    strncpy(users[i].passwd, passwd, MAX_USERNAME - 1);
    users[i].uid = (uint16_t)i;
    sb.s_user_count++;

    user_write_table(users);
    sb_write();
    printf("用户 '%s' 注册成功 (uid=%u)\n", name, i);
    return (int)i;
}

/*
 * 权限检查
 * op: 0400=读, 0200=写, 0100=执行
 * 返回: 1=允许, 0=拒绝
 */
int check_perm(const struct inode *ip, int op)
{
    uint16_t perm, mode = ip->i_mode & 0x1FF;

    /* root(uid=0)拥有所有权限 */
    if (current_uid == 0) return 1;

    if (current_uid < 0) {
        /* 未登录: 使用other权限 */
        perm = mode & 0x007;
    } else if ((uint16_t)current_uid == ip->i_uid) {
        /* 所有者 */
        perm = (mode >> 6) & 0x7;
    } else {
        /* 其他用户 */
        perm = mode & 0x7;
    }

    if ((op & 0400) && !(perm & 0x4)) return 0;
    if ((op & 0200) && !(perm & 0x2)) return 0;
    if ((op & 0100) && !(perm & 0x1)) return 0;
    return 1;
}

/*
 * 权限转为字符串 (如 "rwxr-xr-x")
 */
void perm_to_str(uint16_t mode, char *str)
{
    static const char rwx[] = "rwx";
    int i;
    for (i = 0; i < 9; i++)
        str[i] = (mode & (0400 >> i)) ? rwx[i % 3] : '-';
    str[9] = '\0';
}
