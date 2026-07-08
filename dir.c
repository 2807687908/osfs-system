#include "fs.h"

/* 外部函数声明 */
extern void perm_to_str(uint16_t mode, char *str);
int user_verify(const char *name, const char *passwd);

static char dir_buf[BLOCK_SIZE];

/*
 * 列目录: 显示文件名、物理地址、保护码、文件长度
 */
int dir_list(uint32_t dir_ino)
{
    struct inode dip;
    uint32_t offset = 0;
    int count = 0;

    read_inode(dir_ino, &dip);

    /* 检查类型 */
    if ((dip.i_mode & 0xF000) >> 12 != FT_DIRECTORY && dip.i_mode < 0x100) {
        /* i_mode low bits store type+perm, type is in the upper part of low 16 bits */
    }
    /* 简单判断: i_mode高4位 */
    if (((dip.i_mode >> 12) & 0xF) != FT_DIRECTORY) {
        /* 也可能没有用高4位分离，用另一种方式 */
        if (dip.i_links_count == 0 && dip.i_size == 0) return 0;
    }

    if (!check_perm(&dip, 0400)) {
        printf("权限不足: 无法读取目录\n");
        return -1;
    }

    printf("%-24s %-8s %-10s %-8s %-8s %-8s %s\n",
           "文件名", "Inode", "保护码", "大小(字节)", "物理块号", "逻辑块号", "类型");
    printf("-------------------------------------------------------------------------------\n");

    while (offset < dip.i_size) {
        struct dir_entry de_buf;
        char name[MAX_FILENAME + 1];
        struct inode entry_inode;

        /* 读取目录项头部 */
        {
            uint32_t blk = offset / BLOCK_SIZE;
            uint32_t off = offset % BLOCK_SIZE;
            uint32_t phys = get_block_by_offset(dir_ino, blk * BLOCK_SIZE, 0);
            if (phys == 0) break;
            read_block(phys, dir_buf);
            memcpy(&de_buf, dir_buf + off, sizeof(struct dir_entry));
        }

        if (de_buf.inode == 0) {
            offset += de_buf.rec_len;
            continue;
        }

        memset(name, 0, sizeof(name));
        {
            uint32_t blk = offset / BLOCK_SIZE;
            uint32_t off = offset % BLOCK_SIZE;
            uint32_t phys = get_block_by_offset(dir_ino, blk * BLOCK_SIZE, 0);
            read_block(phys, dir_buf);
            memcpy(name, dir_buf + off + sizeof(struct dir_entry),
                   de_buf.name_len < MAX_FILENAME ? de_buf.name_len : MAX_FILENAME);
        }

        /* 读取inode获取详细信息 */
        read_inode(de_buf.inode, &entry_inode);
        {
            char perm_str[10];
            perm_to_str(entry_inode.i_mode & 0x1FF, perm_str);
            
            uint32_t logical_blk = 0;
            uint32_t phys_blk = 0;
            if (entry_inode.i_size > 0) {
                phys_blk = get_block_by_offset(de_buf.inode, 0, 0);
            }
            
            printf("%-24s %-8u %-10s %-8u %-8u %-8u %s\n",
                   name,
                   de_buf.inode,
                   perm_str,
                   entry_inode.i_size,
                   phys_blk,
                   logical_blk,
                   (de_buf.file_type == FT_DIRECTORY) ? "<DIR>" : "<FILE>");
        }

        count++;
        offset += de_buf.rec_len;
        if (de_buf.rec_len == 0) break;
    }

    if (count == 0) {
        printf("(空目录)\n");
    }
    printf("共 %d 项\n", count);
    return 0;
}

/*
 * 在目录中查找指定名称的目录项
 * 返回其inode号，失败返回0
 */
uint32_t dir_lookup(uint32_t dir_ino, const char *name)
{
    struct inode dip;
    uint32_t offset = 0;

    read_inode(dir_ino, &dip);

    while (offset < dip.i_size) {
        struct dir_entry de_buf;
        char entry_name[MAX_FILENAME + 1];
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys;

        phys = get_block_by_offset(dir_ino, blk * BLOCK_SIZE, 0);
        if (phys == 0) break;
        read_block(phys, dir_buf);
        memcpy(&de_buf, dir_buf + off, sizeof(struct dir_entry));

        if (de_buf.inode == 0) {
            offset += de_buf.rec_len;
            if (de_buf.rec_len == 0) break;
            continue;
        }

        memset(entry_name, 0, sizeof(entry_name));
        {
            /* 重新读取确保获取name部分 */
            read_block(phys, dir_buf);
            memcpy(entry_name, dir_buf + off + sizeof(struct dir_entry),
                   de_buf.name_len < MAX_FILENAME ? de_buf.name_len : MAX_FILENAME);
        }

        if (strcmp(entry_name, name) == 0) {
            return de_buf.inode;
        }

        offset += de_buf.rec_len;
        if (de_buf.rec_len == 0) break;
    }
    return 0;
}

/*
 * 在目录中添加目录项
 */
void dir_add_entry(uint32_t dir_ino, uint32_t ino, const char *name, uint8_t ftype)
{
    struct inode dip;
    uint32_t offset = 0;
    int entry_size;
    int name_len = (int)strlen(name);

    entry_size = (int)(sizeof(struct dir_entry) + name_len);
    /* 4字节对齐 */
    entry_size = (entry_size + 3) & ~3;

    read_inode(dir_ino, &dip);

    /* 确保有空间写入 */
    {
        uint32_t needed = dip.i_size + (uint32_t)entry_size;
        /* 通过get_block_by_offset确保块已分配 */
        get_block_by_offset(dir_ino, needed, 1);
    }

    /* 重新读取inode，因为get_block_by_offset可能已经修改了它 */
    read_inode(dir_ino, &dip);

    /* 在目录末尾添加 */
    offset = dip.i_size;

    {
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys = get_block_by_offset(dir_ino, blk * BLOCK_SIZE, 0);

        if (phys == 0) {
            fprintf(stderr, "[ERROR] 无法为目录项分配块\n");
            return;
        }

        read_block(phys, dir_buf);

        /* 如果跨块，需要特殊处理 */
        if (off + entry_size > BLOCK_SIZE) {
            /* 简化: 补充分配下一个块 */
            get_block_by_offset(dir_ino, (blk + 1) * BLOCK_SIZE, 1);
            phys = get_block_by_offset(dir_ino, blk * BLOCK_SIZE, 0);
            read_block(phys, dir_buf);

            if (off + (int)sizeof(struct dir_entry) <= BLOCK_SIZE) {
                /* 头部在当前块，名字跨到下一块 */
                struct dir_entry *de = (struct dir_entry *)(dir_buf + off);
                uint32_t next_phys;

                de->inode = ino;
                de->rec_len = (uint16_t)(BLOCK_SIZE - off);  /* 到块尾 */
                de->name_len = (uint8_t)name_len;
                de->file_type = ftype;
                memcpy(dir_buf + off + sizeof(struct dir_entry), name,
                       BLOCK_SIZE - off - (int)sizeof(struct dir_entry));
                write_block(phys, dir_buf);

                /* 名字剩余部分写入下一块 */
                next_phys = get_block_by_offset(dir_ino, (blk + 1) * BLOCK_SIZE, 0);
                {
                    char tmp[BLOCK_SIZE];
                    memset(tmp, 0, BLOCK_SIZE);
                    memcpy(tmp, name + (BLOCK_SIZE - off - sizeof(struct dir_entry)),
                           name_len - (BLOCK_SIZE - off - (int)sizeof(struct dir_entry)));
                    /* 填充剩余空间 */
                    {
                        struct dir_entry *next_de = (struct dir_entry *)tmp;
                        next_de->inode = 0;
                        next_de->rec_len = (uint16_t)(BLOCK_SIZE);
                        next_de->name_len = 0;
                        next_de->file_type = FT_UNKNOWN;
                    }
                    write_block(next_phys, tmp);
                }
            }
        } else {
            struct dir_entry *de = (struct dir_entry *)(dir_buf + off);
            de->inode = ino;
            de->rec_len = (uint16_t)entry_size;
            de->name_len = (uint8_t)name_len;
            de->file_type = ftype;
            memcpy(dir_buf + off + sizeof(struct dir_entry), name, name_len);
            write_block(phys, dir_buf);
        }
    }

    dip.i_size += entry_size;
    dip.i_mtime = (uint32_t)time(NULL);
    write_inode(dir_ino, &dip);
}

/*
 * 从目录中删除指定名称的目录项
 */
void dir_remove_entry(uint32_t dir_ino, const char *name)
{
    struct inode dip;
    uint32_t offset = 0;
    uint32_t prev_offset = 0;
    int found = 0;

    read_inode(dir_ino, &dip);

    while (offset < dip.i_size) {
        struct dir_entry de_buf;
        char entry_name[MAX_FILENAME + 1];
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys;

        phys = get_block_by_offset(dir_ino, blk * BLOCK_SIZE, 0);
        if (phys == 0) break;
        read_block(phys, dir_buf);
        memcpy(&de_buf, dir_buf + off, sizeof(struct dir_entry));

        if (de_buf.inode == 0) {
            prev_offset = offset;
            offset += de_buf.rec_len;
            continue;
        }

        memset(entry_name, 0, sizeof(entry_name));
        {
            read_block(phys, dir_buf);
            memcpy(entry_name, dir_buf + off + sizeof(struct dir_entry),
                   de_buf.name_len < MAX_FILENAME ? de_buf.name_len : MAX_FILENAME);
        }

        if (strcmp(entry_name, name) == 0) {
            found = 1;
            break;
        }

        prev_offset = offset;
        offset += de_buf.rec_len;
        if (de_buf.rec_len == 0) break;
    }

    if (!found) {
        printf("未找到 '%s'\n", name);
        return;
    }

    /* 将该目录项的inode设为0标记删除 */
    {
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys = get_block_by_offset(dir_ino, blk * BLOCK_SIZE, 0);
        struct dir_entry *de;

        read_block(phys, dir_buf);
        de = (struct dir_entry *)(dir_buf + off);
        de->inode = 0;

        /* 如果是最后一个条目，扩大前一个条目的rec_len */
        if (offset + de->rec_len >= dip.i_size) {
            if (prev_offset != offset) {
                /* 读取前一个条目 */
                uint32_t prev_blk = prev_offset / BLOCK_SIZE;
                uint32_t prev_off = prev_offset % BLOCK_SIZE;
                uint32_t prev_phys = get_block_by_offset(dir_ino, prev_blk * BLOCK_SIZE, 0);
                char prev_buf[BLOCK_SIZE];
                struct dir_entry *prev_de;
                read_block(prev_phys, prev_buf);
                prev_de = (struct dir_entry *)(prev_buf + prev_off);
                prev_de->rec_len += de->rec_len;
                write_block(prev_phys, prev_buf);
            }
            dip.i_size = offset;  /* 缩减目录大小 */
        }

        write_block(phys, dir_buf);
    }

    dip.i_mtime = (uint32_t)time(NULL);
    write_inode(dir_ino, &dip);
}

/*
 * 创建目录
 */
int dir_create(const char *name, uint32_t parent_ino)
{
    struct inode pip, new_dip;
    uint32_t new_ino;
    time_t now = time(NULL);

    /* 检查是否已存在 */
    if (dir_lookup(parent_ino, name) != 0) {
        printf("目录 '%s' 已存在\n", name);
        return -1;
    }

    /* 检查父目录写权限 */
    read_inode(parent_ino, &pip);
    if (!check_perm(&pip, 0200)) {
        printf("权限不足: 无法在目录中创建子目录\n");
        return -1;
    }

    /* 分配inode */
    new_ino = (uint32_t)inode_alloc();
    if (new_ino == (uint32_t)-1) return -1;

    /* 初始化新目录的inode */
    memset(&new_dip, 0, sizeof(new_dip));
    new_dip.i_mode = (FT_DIRECTORY << 12) | DEFAULT_DIR_PERM;
    new_dip.i_uid = (uint16_t)(current_uid >= 0 ? current_uid : 0);
    new_dip.i_size = 0;
    new_dip.i_atime = (uint32_t)now;
    new_dip.i_ctime = (uint32_t)now;
    new_dip.i_mtime = (uint32_t)now;
    new_dip.i_links_count = 2;
    new_dip.i_blocks = 1;

    /* 为新目录分配数据块 */
    {
        int blk = block_alloc();
        if (blk < 0) {
            inode_free(new_ino);
            return -1;
        }
        new_dip.i_block[0] = (uint32_t)blk;

        /* 写入 . 和 .. */
        char buf[BLOCK_SIZE];
        struct dir_entry *de;
        int dot_entry_size, dotdot_entry_size;

        memset(buf, 0, BLOCK_SIZE);

        /* . */
        de = (struct dir_entry *)buf;
        de->inode = new_ino;
        dot_entry_size = (int)(sizeof(struct dir_entry) + 1);
        dot_entry_size = (dot_entry_size + 3) & ~3;
        de->rec_len = (uint16_t)dot_entry_size;
        de->name_len = 1;
        de->file_type = FT_DIRECTORY;
        *(char *)(de + 1) = '.';

        /* .. */
        dotdot_entry_size = (int)(sizeof(struct dir_entry) + 2);
        dotdot_entry_size = (dotdot_entry_size + 3) & ~3;
        de = (struct dir_entry *)(buf + dot_entry_size);
        de->inode = parent_ino;
        de->rec_len = (uint16_t)dotdot_entry_size;  /* 实际大小 */
        de->name_len = 2;
        de->file_type = FT_DIRECTORY;
        memcpy((char *)(de + 1), "..", 2);

        write_block((uint32_t)blk, buf);
        new_dip.i_size = (uint32_t)(dot_entry_size + dotdot_entry_size);
    }

    write_inode(new_ino, &new_dip);

    /* 在父目录添加条目 */
    dir_add_entry(parent_ino, new_ino, name, FT_DIRECTORY);

    /* 父目录链接数+1 (重新读取，因为dir_add_entry已更新了父目录inode) */
    {
        struct inode tmp_pip;
        read_inode(parent_ino, &tmp_pip);
        tmp_pip.i_links_count++;
        write_inode(parent_ino, &tmp_pip);
    }

    journal_log_create(new_ino);

    printf("目录 '%s' 创建成功 (inode=%u)\n", name, new_ino);
    return 0;
}

/*
 * 删除空目录
 */
int dir_remove(const char *name, uint32_t parent_ino)
{
    struct inode dip;
    uint32_t ino;

    ino = dir_lookup(parent_ino, name);
    if (ino == 0) {
        printf("目录 '%s' 不存在\n", name);
        return -1;
    }

    read_inode(ino, &dip);

    /* 检查是否为目录 */
    if (((dip.i_mode >> 12) & 0xF) != FT_DIRECTORY) {
        printf("'%s' 不是目录\n", name);
        return -1;
    }

    /* 检查权限 */
    if (!check_perm(&dip, 0200)) {
        printf("权限不足\n");
        return -1;
    }

    /* 检查父目录写权限 */
    {
        struct inode pip;
        read_inode(parent_ino, &pip);
        if (!check_perm(&pip, 0200)) {
            printf("权限不足: 无法从父目录删除\n");
            return -1;
        }
    }

    /* 检查目录是否为空(只有.和..) */
    /* . 条目: sizeof(dir_entry) + 1 对齐后为16字节 */
    /* .. 条目: sizeof(dir_entry) + 2 对齐后为16字节 */
    /* 空目录总大小为 32 字节 */
    if (dip.i_size > 32) {
        printf("目录 '%s' 不为空，无法删除\n", name);
        return -1;
    }

    /* 父目录链接数-1 */
    {
        struct inode pip;
        read_inode(parent_ino, &pip);
        pip.i_links_count--;
        write_inode(parent_ino, &pip);
    }

    /* 从父目录移除条目 */
    dir_remove_entry(parent_ino, name);

    journal_log_delete(ino);

    /* 释放inode(内部会截断并释放数据块) */
    inode_free(ino);

    printf("目录 '%s' 已删除\n", name);
    return 0;
}

/*
 * 切换目录
 */
int dir_change(const char *path)
{
    uint32_t target_ino;
    struct inode tip;

    if (path_resolve(path, &target_ino) != 0) {
        printf("路径 '%s' 不存在\n", path);
        return -1;
    }
    
    /* 根目录的父目录是自身，cd .. 应该保持在根目录 */
    if (current_dir == 1 && strcmp(path, "..") == 0) {
        printf("当前目录: %s\n", current_path);
        return 0;
    }

    read_inode(target_ino, &tip);
    if (((tip.i_mode >> 12) & 0xF) != FT_DIRECTORY) {
        printf("'%s' 不是目录\n", path);
        return -1;
    }

    if (!check_perm(&tip, 0100)) {
        printf("权限不足: 无法进入该目录\n");
        return -1;
    }

    current_dir = target_ino;

    /* 更新当前路径 */
    if (strcmp(path, "/") == 0) {
        strcpy(current_path, "/");
    } else if (path[0] == '/') {
        strncpy(current_path, path, MAX_PATH - 1);
        current_path[MAX_PATH - 1] = '\0';
    } else {
        if (strcmp(current_path, "/") == 0) {
            snprintf(current_path, MAX_PATH, "/%s", path);
        } else {
            if (path[0] == '.' && path[1] == '.' && path[2] == '\0') {
                /* 上级目录 */
                char *slash = strrchr(current_path, '/');
                if (slash && slash != current_path) {
                    *slash = '\0';
                } else if (slash) {
                    *(slash + 1) = '\0';
                }
            } else {
                size_t len = strlen(current_path);
                if (len + strlen(path) + 2 < MAX_PATH) {
                    if (current_path[len - 1] != '/')
                        strcat(current_path, "/");
                    strcat(current_path, path);
                }
            }
        }
    }

    printf("当前目录: %s\n", current_path);
    return 0;
}

/*
 * 路径解析: 将路径字符串解析为inode号
 * 支持绝对路径(/开头)和相对路径
 */
int path_resolve(const char *path, uint32_t *ino)
{
    uint32_t current;
    char *token, *saveptr;
    char work[MAX_PATH];

    if (path == NULL || path[0] == '\0') {
        *ino = current_dir;
        return 0;
    }

    /* 判断绝对/相对路径 */
    if (path[0] == '/') {
        current = 1;  /* 从根目录开始 */
        strncpy(work, path + 1, MAX_PATH - 1);
    } else {
        current = current_dir;
        strncpy(work, path, MAX_PATH - 1);
    }
    work[MAX_PATH - 1] = '\0';

    if (work[0] == '\0') {
        *ino = current;
        return 0;
    }

    /* 逐级解析 */
    token = strtok_r(work, "/", &saveptr);
    while (token != NULL) {
        uint32_t next;

        /* 处理 . 和 .. */
        if (strcmp(token, ".") == 0) {
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }
        if (strcmp(token, "..") == 0) {
            /* 查找父目录 */
            next = dir_lookup(current, "..");
            if (next == 0) {
                return -1;
            }
            current = next;
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        next = dir_lookup(current, token);
        if (next == 0) {
            return -1;
        }
        current = next;
        token = strtok_r(NULL, "/", &saveptr);
    }

    *ino = current;
    return 0;
}
