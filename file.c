#include "fs.h"

static char file_buf[BLOCK_SIZE];

/*
 * 创建文件
 */
int file_create(const char *name, uint32_t parent_ino, uint16_t perm)
{
    struct inode pip, new_ip;
    uint32_t new_ino;
    time_t now = time(NULL);

    if (strlen(name) == 0 || strlen(name) > MAX_FILENAME) {
        printf("文件名无效\n");
        return -1;
    }

    /* 检查父目录是否存在且有写权限 */
    read_inode(parent_ino, &pip);
    if (!check_perm(&pip, 0200)) {
        printf("权限不足: 无法创建文件\n");
        return -1;
    }

    /* 检查同名文件是否已存在 */
    if (dir_lookup(parent_ino, name) != 0) {
        printf("文件 '%s' 已存在\n", name);
        return -1;
    }

    /* 分配inode */
    new_ino = (uint32_t)inode_alloc();
    if (new_ino == (uint32_t)-1) return -1;

    /* 初始化inode */
    memset(&new_ip, 0, sizeof(new_ip));
    new_ip.i_mode = (FT_REG_FILE << 12) | (perm & 0x1FF);
    new_ip.i_uid = (uint16_t)(current_uid >= 0 ? current_uid : 0);
    new_ip.i_size = 0;
    new_ip.i_atime = (uint32_t)now;
    new_ip.i_ctime = (uint32_t)now;
    new_ip.i_mtime = (uint32_t)now;
    new_ip.i_links_count = 1;
    new_ip.i_blocks = 0;

    write_inode(new_ino, &new_ip);

    /* 在父目录添加条目 */
    dir_add_entry(parent_ino, new_ino, name, FT_REG_FILE);

    journal_log_create(new_ino);

    printf("文件 '%s' 创建成功 (inode=%u, 权限=%o)\n", name, new_ino, perm & 0x1FF);
    return 0;
}

/*
 * 删除文件
 */
int file_delete(const char *name, uint32_t parent_ino)
{
    struct inode ip;
    uint32_t ino;

    ino = dir_lookup(parent_ino, name);
    if (ino == 0) {
        printf("文件 '%s' 不存在\n", name);
        return -1;
    }

    read_inode(ino, &ip);

    /* 检查是否为普通文件(不是目录) */
    if (((ip.i_mode >> 12) & 0xF) == FT_DIRECTORY) {
        printf("'%s' 是目录，请使用 rmdir 命令\n", name);
        return -1;
    }

    /* 检查权限 */
    if (!check_perm(&ip, 0200)) {
        printf("权限不足: 无法删除文件\n");
        return -1;
    }

    /* 检查父目录权限 */
    {
        struct inode pip;
        read_inode(parent_ino, &pip);
        if (!check_perm(&pip, 0200)) {
            printf("权限不足: 无法从目录中删除\n");
            return -1;
        }
    }

    /* 从目录中移除 */
    dir_remove_entry(parent_ino, name);

    journal_log_delete(ino);

    /* 释放inode(内部会截断并释放数据块) */
    inode_free(ino);

    printf("文件 '%s' 已删除\n", name);
    return 0;
}

/*
 * 打开文件
 * 返回文件描述符编号，失败返回-1
 */
int file_open(const char *name, uint16_t mode)
{
    struct inode ip;
    uint32_t ino;
    int fd, i;

    /* 查找文件 */
    ino = dir_lookup(current_dir, name);
    if (ino == 0) {
        /* 尝试解析为路径 */
        uint32_t parent;
        const char *fname;
        char path_copy[MAX_PATH];
        char *slash;

        strncpy(path_copy, name, MAX_PATH - 1);
        path_copy[MAX_PATH - 1] = '\0';

        slash = strrchr(path_copy, '/');
        if (slash) {
            *slash = '\0';
            fname = slash + 1;
            if (path_resolve(path_copy, &parent) != 0) {
                printf("路径 '%s' 不存在\n", path_copy);
                return -1;
            }
            ino = dir_lookup(parent, fname);
        }
        if (ino == 0) {
            printf("文件 '%s' 不存在\n", name);
            return -1;
        }
    }

    read_inode(ino, &ip);

    /* 检查类型 */
    if (((ip.i_mode >> 12) & 0xF) == FT_DIRECTORY) {
        printf("'%s' 是目录，无法打开\n", name);
        return -1;
    }

    /* 检查权限 */
    if (mode == OPEN_READ || mode == OPEN_RDWR) {
        if (!check_perm(&ip, 0400)) {
            printf("权限不足: 无读权限\n");
            return -1;
        }
    }
    if (mode == OPEN_WRITE || mode == OPEN_RDWR) {
        if (!check_perm(&ip, 0200)) {
            printf("权限不足: 无写权限\n");
            return -1;
        }
    }

    /* 分配文件描述符 */
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (!fd_table[i].used) break;
    }
    if (i >= MAX_OPEN_FILES) {
        printf("打开文件数已达上限(%d)\n", MAX_OPEN_FILES);
        return -1;
    }

    fd = i;
    fd_table[fd].used = 1;
    fd_table[fd].inode_no = ino;
    fd_table[fd].offset = 0;
    fd_table[fd].mode = mode;

    /* 更新访问时间 */
    ip.i_atime = (uint32_t)time(NULL);
    write_inode(ino, &ip);

    printf("已打开文件 '%s', fd=%d, 模式=%s\n",
           name, fd,
           mode == OPEN_READ ? "r" : (mode == OPEN_WRITE ? "w" : "rw"));
    return fd;
}

/*
 * 关闭文件
 */
int file_close(int fd)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].used) {
        printf("无效的文件描述符: %d\n", fd);
        return -1;
    }

    fd_table[fd].used = 0;
    fd_table[fd].inode_no = 0;
    fd_table[fd].offset = 0;

    printf("文件描述符 %d 已关闭\n", fd);
    return 0;
}

/*
 * 从文件读取
 */
int file_read(int fd, char *buf, int size)
{
    struct inode ip;
    uint32_t ino;
    uint32_t offset;
    int total = 0;
    int remaining;

    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].used) {
        printf("无效的文件描述符: %d\n", fd);
        return -1;
    }

    if (fd_table[fd].mode != OPEN_READ && fd_table[fd].mode != OPEN_RDWR) {
        printf("文件未以读模式打开\n");
        return -1;
    }

    ino = fd_table[fd].inode_no;
    offset = fd_table[fd].offset;

    read_inode(ino, &ip);

    if (!check_perm(&ip, 0400)) {
        printf("权限不足\n");
        return -1;
    }

    remaining = (int)(ip.i_size - offset);
    if (remaining <= 0) {
        return 0;  /* EOF */
    }
    if (size > remaining) size = remaining;

    while (total < size) {
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys;
        int chunk;

        phys = get_block_by_offset(ino, blk * BLOCK_SIZE, 0);
        if (phys == 0) break;

        read_block(phys, file_buf);

        chunk = (int)(BLOCK_SIZE - off);
        if (chunk > size - total) chunk = size - total;

        memcpy(buf + total, file_buf + off, chunk);
        total += chunk;
        offset += chunk;
    }

    fd_table[fd].offset = offset;

    /* 更新访问时间 */
    ip.i_atime = (uint32_t)time(NULL);
    write_inode(ino, &ip);

    return total;
}

/*
 * 向文件写入
 */
int file_write(int fd, const char *buf, int size)
{
    struct inode ip;
    uint32_t ino;
    uint32_t offset;
    int total = 0;

    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].used) {
        printf("无效的文件描述符: %d\n", fd);
        return -1;
    }

    if (fd_table[fd].mode != OPEN_WRITE && fd_table[fd].mode != OPEN_RDWR) {
        printf("文件未以写模式打开\n");
        return -1;
    }

    ino = fd_table[fd].inode_no;
    offset = fd_table[fd].offset;

    read_inode(ino, &ip);

    if (!check_perm(&ip, 0200)) {
        printf("权限不足\n");
        return -1;
    }

    while (total < size) {
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys;
        int chunk;

        phys = get_block_by_offset(ino, blk * BLOCK_SIZE, 1);  /* alloc=1 */
        if (phys == 0) {
            printf("磁盘空间不足\n");
            break;
        }

        read_block(phys, file_buf);

        chunk = (int)(BLOCK_SIZE - off);
        if (chunk > size - total) chunk = size - total;

        memcpy(file_buf + off, buf + total, chunk);
        write_block(phys, file_buf);

        total += chunk;
        offset += chunk;
    }

    fd_table[fd].offset = offset;

    /* 重新读取inode（get_block_by_offset可能已更新了i_block） */
    read_inode(ino, &ip);

    /* 更新文件大小和时间 */
    if (offset > ip.i_size) {
        ip.i_size = offset;
    }
    ip.i_mtime = (uint32_t)time(NULL);
    ip.i_atime = (uint32_t)time(NULL);
    write_inode(ino, &ip);

    return total;
}

/*
 * 修改文件权限
 */
int file_chmod(const char *name, uint32_t parent_ino, uint16_t new_perm)
{
    struct inode ip;
    uint32_t ino;

    ino = dir_lookup(parent_ino, name);
    if (ino == 0) {
        printf("文件 '%s' 不存在\n", name);
        return -1;
    }

    read_inode(ino, &ip);

    /* 只有文件所有者和root可以修改权限 */
    if (current_uid != 0 && (uint16_t)current_uid != ip.i_uid) {
        printf("权限不足: 只有文件所有者和root可以修改权限\n");
        return -1;
    }

    ip.i_mode = (ip.i_mode & 0xF000) | (new_perm & 0x1FF);
    ip.i_mtime = (uint32_t)time(NULL);
    write_inode(ino, &ip);

    journal_log_chmod(ino, new_perm);

    printf("文件 '%s' 权限已修改为 %o\n", name, new_perm & 0x1FF);
    return 0;
}
