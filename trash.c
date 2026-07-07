#include "fs.h"

#define TRASH_DIR_NAME ".trash"
#define TRASH_MAX_ITEMS 1024

struct trash_item {
    char original_path[MAX_PATH];
    char name[MAX_FILENAME];
    uint32_t original_inode;
    uint32_t timestamp;
    uint8_t file_type;
};

static uint32_t trash_dir_ino = 0;

static uint32_t get_trash_dir(void)
{
    if (trash_dir_ino != 0) return trash_dir_ino;
    
    trash_dir_ino = dir_lookup(1, TRASH_DIR_NAME);
    if (trash_dir_ino == 0) {
        int old_uid = current_uid;
        current_uid = 0;
        dir_create(TRASH_DIR_NAME, 1);
        current_uid = old_uid;
        trash_dir_ino = dir_lookup(1, TRASH_DIR_NAME);
    }
    return trash_dir_ino;
}

static int read_trash_items(struct trash_item *items, int max_items)
{
    struct inode dip;
    uint32_t offset = 0;
    int count = 0;
    uint32_t dir_ino = get_trash_dir();
    
    read_inode(dir_ino, &dip);
    
    while (offset < dip.i_size && count < max_items) {
        struct dir_entry de_buf;
        char name[MAX_FILENAME + 1];
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys;
        char buf[BLOCK_SIZE];
        
        phys = get_block_by_offset(dir_ino, blk * BLOCK_SIZE, 0);
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
        
        items[count].file_type = de_buf.file_type;
        items[count].original_inode = de_buf.inode;
        strncpy(items[count].name, name, MAX_FILENAME - 1);
        items[count].timestamp = (uint32_t)time(NULL);
        count++;
        
        offset += de_buf.rec_len;
        if (de_buf.rec_len == 0) break;
    }
    
    return count;
}

int trash_move(const char *path)
{
    uint32_t ino, parent_ino;
    struct inode ip;
    char name[MAX_FILENAME];
    char new_name[MAX_FILENAME];
    char *slash;
    uint32_t trash_dir = get_trash_dir();
    struct inode trash_ip;
    
    if (path_resolve(path, &ino) != 0) {
        printf("文件/目录 '%s' 不存在\n", path);
        return -1;
    }
    
    read_inode(ino, &ip);
    
    slash = strrchr(path, '/');
    if (slash) {
        strncpy(name, slash + 1, MAX_FILENAME - 1);
    } else {
        strncpy(name, path, MAX_FILENAME - 1);
    }
    
    snprintf(new_name, MAX_FILENAME, "%s_%u", name, (unsigned int)time(NULL));
    
    dir_add_entry(trash_dir, ino, new_name, 
                  ((ip.i_mode >> 12) & 0xF) == FT_DIRECTORY ? FT_DIRECTORY : FT_REG_FILE);
    
    read_inode(trash_dir, &trash_ip);
    trash_ip.i_links_count++;
    write_inode(trash_dir, &trash_ip);
    
    path_resolve(path, &ino);
    slash = strrchr(path, '/');
    char parent_path[MAX_PATH];
    if (slash) {
        strncpy(parent_path, path, slash - path);
        parent_path[slash - path] = '\0';
        path_resolve(parent_path, &parent_ino);
    } else {
        parent_ino = current_dir;
    }
    
    dir_remove_entry(parent_ino, name);
    
    printf("已将 '%s' 移入回收站\n", name);
    return 0;
}

int trash_list(void)
{
    struct trash_item items[TRASH_MAX_ITEMS];
    int count = read_trash_items(items, TRASH_MAX_ITEMS);
    
    printf("回收站内容:\n");
    printf("%-24s %-12s %-16s %s\n", "文件名", "类型", "删除时间", "原始Inode");
    printf("-------------------------------------------------------------------\n");
    
    if (count == 0) {
        printf("(空)\n");
        return 0;
    }
    
    for (int i = 0; i < count; i++) {
        char time_str[20];
        struct tm *t = localtime((time_t *)&items[i].timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
        printf("%-24s %-12s %-16s %u\n",
               items[i].name,
               items[i].file_type == FT_DIRECTORY ? "<DIR>" : "<FILE>",
               time_str,
               items[i].original_inode);
    }
    printf("共 %d 项\n", count);
    return 0;
}

int trash_restore(const char *name)
{
    uint32_t trash_dir = get_trash_dir();
    uint32_t ino = dir_lookup(trash_dir, name);
    
    if (ino == 0) {
        printf("回收站中未找到 '%s'\n", name);
        return -1;
    }
    
    struct inode ip;
    read_inode(ino, &ip);
    
    char restore_name[MAX_FILENAME];
    char *underscore = strrchr(name, '_');
    if (underscore) {
        strncpy(restore_name, name, underscore - name);
        restore_name[underscore - name] = '\0';
    } else {
        strncpy(restore_name, name, MAX_FILENAME - 1);
    }
    
    int exists = dir_lookup(current_dir, restore_name);
    if (exists != 0) {
        printf("目标位置已存在同名文件 '%s'\n", restore_name);
        return -1;
    }
    
    dir_add_entry(current_dir, ino, restore_name,
                  ((ip.i_mode >> 12) & 0xF) == FT_DIRECTORY ? FT_DIRECTORY : FT_REG_FILE);
    
    dir_remove_entry(trash_dir, name);
    
    printf("已将 '%s' 恢复到当前目录\n", restore_name);
    return 0;
}

int trash_empty(void)
{
    uint32_t trash_dir = get_trash_dir();
    struct inode dip;
    uint32_t offset = 0;
    int count = 0;
    
    read_inode(trash_dir, &dip);
    
    while (offset < dip.i_size) {
        struct dir_entry de_buf;
        char name[MAX_FILENAME + 1];
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys;
        char buf[BLOCK_SIZE];
        uint32_t next_offset = offset;
        
        phys = get_block_by_offset(trash_dir, blk * BLOCK_SIZE, 0);
        if (phys == 0) break;
        read_block(phys, buf);
        memcpy(&de_buf, buf + off, sizeof(struct dir_entry));
        
        if (de_buf.inode != 0) {
            memset(name, 0, sizeof(name));
            memcpy(name, buf + off + sizeof(struct dir_entry),
                   de_buf.name_len < MAX_FILENAME ? de_buf.name_len : MAX_FILENAME);
            
            inode_free(de_buf.inode);
            count++;
        }
        
        next_offset += de_buf.rec_len;
        if (de_buf.rec_len == 0) break;
        offset = next_offset;
    }
    
    inode_truncate(&dip, 0);
    dip.i_links_count = 2;
    write_inode(trash_dir, &dip);
    
    printf("回收站已清空，共删除 %d 项\n", count);
    return 0;
}