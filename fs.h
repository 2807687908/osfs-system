#ifndef _FS_H_
#define _FS_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ========== 磁盘布局常量 ========== */
#define BLOCK_SIZE          1024         /* 块大小: 1KB */
#define DISK_SIZE           (32 * 1024 * 1024)  /* 32MB */
#define TOTAL_BLOCKS        32768        /* 总块数 */
#define TOTAL_INODES        1024         /* 总inode数 */
#define INODE_SIZE          128          /* 每个inode 128字节 */
#define INODES_PER_BLOCK    (BLOCK_SIZE / INODE_SIZE)  /* 每块8个inode */

/* 各区域起始块号 */
#define SUPERBLOCK_BLOCK    0
#define BLOCK_BITMAP_START  1
#define BLOCK_BITMAP_BLOCKS 4           /* 32768位/(1024*8)=4块 */
#define INODE_BITMAP_START  5
#define INODE_BITMAP_BLOCKS 1
#define INODE_TABLE_START   6
#define INODE_TABLE_BLOCKS  ((TOTAL_INODES * INODE_SIZE) / BLOCK_SIZE)  /* 128块 */
#define USER_TABLE_START    134         /* 用户表: 块134-135 (2块) */
#define USER_TABLE_BLOCKS   2
#define DATA_START_BLOCK    136         /* 6+128+2=136 */

/* 魔数 */
#define EXT2_MAGIC          0xEF53

/* 文件名限制 */
#define MAX_FILENAME        255
#define MAX_PATH            1024
#define MAX_USERS           16
#define MAX_USERNAME        32
#define MAX_OPEN_FILES      16

/* 文件类型 */
#define FT_UNKNOWN           0
#define FT_REG_FILE          1
#define FT_DIRECTORY         2

/* 权限位 */
#define PERM_IRUSR          0400
#define PERM_IWUSR          0200
#define PERM_IXUSR          0100
#define PERM_IRGRP          0040
#define PERM_IWGRP          0020
#define PERM_IXGRP          0010
#define PERM_IROTH          0004
#define PERM_IWOTH          0002
#define PERM_IXOTH          0001

#define DEFAULT_FILE_PERM   0644
#define DEFAULT_DIR_PERM    0755

/* 打开模式 */
#define OPEN_READ            1
#define OPEN_WRITE           2
#define OPEN_RDWR            3

/* ========== 确保结构体紧凑 ========== */
#pragma pack(push, 1)

/* 超级块 — 128字节，位于块0开头 */
struct superblock {
    uint32_t s_inodes_count;          /* inode总数 */
    uint32_t s_blocks_count;          /* 块总数 */
    uint32_t s_free_blocks_count;     /* 空闲块数 */
    uint32_t s_free_inodes_count;     /* 空闲inode数 */
    uint32_t s_first_data_block;      /* 第一个数据块 */
    uint32_t s_log_block_size;        /* log2(块大小/1024) */
    uint32_t s_magic;                 /* 魔数 */
    uint32_t s_root_ino;              /* 根目录inode号=1 */
    uint32_t s_user_count;            /* 已注册用户数 */
    uint32_t s_user_table_start;      /* 用户表起始块号 */
    uint32_t s_pad[22];               /* 填充至128字节 */
};

/* 组描述符 — 32字节，紧跟超级块 */
struct group_desc {
    uint32_t bg_block_bitmap;         /* 块位图起始块 */
    uint32_t bg_inode_bitmap;         /* inode位图起始块 */
    uint32_t bg_inode_table;          /* inode表起始块 */
    uint32_t bg_free_blocks_count;
    uint32_t bg_free_inodes_count;
    uint32_t bg_pad[3];
};

/* 用户条目 — 68字节 */
struct user_entry {
    char     name[MAX_USERNAME];      /* 用户名 */
    char     passwd[MAX_USERNAME];    /* 密码 */
    uint16_t uid;                     /* 用户ID */
    uint16_t _pad;                    /* 对齐 */
};

/* Inode — 128字节 */
struct inode {
    uint16_t i_mode;                  /* 文件类型+权限 */
    uint16_t i_uid;                   /* 所有者ID */
    uint32_t i_size;                  /* 文件大小(字节) */
    uint32_t i_atime;                 /* 访问时间 */
    uint32_t i_ctime;                 /* 创建时间 */
    uint32_t i_mtime;                 /* 修改时间 */
    uint16_t i_links_count;           /* 硬链接数 */
    uint32_t i_blocks;                /* 占用的块数 */
    uint32_t i_block[15];             /* 12直接+1间接+1双间接+1三间接 */
    char     i_pad[42];               /* 填充至128字节 (2+2+4+4+4+4+2+4+60=86+42=128) */
};

/* 目录项 — 8字节头部 */
struct dir_entry {
    uint32_t inode;                   /* inode号 */
    uint16_t rec_len;                 /* 目录项长度 */
    uint8_t  name_len;                /* 文件名长度 */
    uint8_t  file_type;               /* 文件类型 */
};

#pragma pack(pop)

/* 前置声明 */
struct user_entry;

/* 文件描述符(内存结构) */
struct file_desc {
    uint32_t inode_no;
    uint32_t offset;
    uint16_t mode;
    uint8_t  used;
};

/* ========== 编译时验证结构体大小 ========== */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(struct superblock) == 128, "superblock must be 128 bytes");
_Static_assert(sizeof(struct group_desc) == 32,   "group_desc must be 32 bytes");
_Static_assert(sizeof(struct inode) == 128,       "inode must be 128 bytes");
_Static_assert(sizeof(struct dir_entry) == 8,     "dir_entry must be 8 bytes");
_Static_assert(sizeof(struct user_entry) == 68,   "user_entry must be 68 bytes");
#endif

/* ========== 外部全局变量 ========== */
extern FILE *disk;
extern struct superblock sb;
extern struct group_desc gd;
extern int current_uid;
extern char current_user[MAX_USERNAME];
extern uint32_t current_dir;
extern char current_path[MAX_PATH];
extern struct file_desc fd_table[MAX_OPEN_FILES];

/* ========== fs.c 函数声明 ========== */
void disk_init(void);
void disk_close(void);
void sb_read(void);
void sb_write(void);
int  block_alloc(void);
void block_free(uint32_t block);
int  inode_alloc(void);
void inode_free(uint32_t ino);
void bitmap_set(uint32_t start_block, uint32_t bit_no, int val);
int  bitmap_get(uint32_t start_block, uint32_t bit_no);
void read_block(uint32_t block_no, void *buf);
void write_block(uint32_t block_no, const void *buf);

/* ========== inode.c 函数声明 ========== */
void read_inode(uint32_t ino, struct inode *ip);
void write_inode(uint32_t ino, const struct inode *ip);
uint32_t get_block_by_offset(uint32_t ino, uint32_t offset, int alloc);
int  inode_truncate(struct inode *ip, uint32_t new_size);

/* ========== user.c 函数声明 ========== */
int  user_login(const char *name, const char *passwd);
void user_logout(void);
int  user_register(const char *name, const char *passwd);
int  check_perm(const struct inode *ip, int op);
int  user_verify(const char *name, const char *passwd);
int  user_login_full(const char *name);
void perm_to_str(uint16_t mode, char *str);
void user_read_table(struct user_entry *users);
void user_write_table(const struct user_entry *users);

/* ========== dir.c 函数声明 ========== */
uint32_t dir_lookup(uint32_t dir_ino, const char *name);
int  dir_list(uint32_t dir_ino);
int  dir_create(const char *name, uint32_t parent_ino);
int  dir_remove(const char *name, uint32_t parent_ino);
int  dir_change(const char *path);
int  path_resolve(const char *path, uint32_t *ino);
void dir_add_entry(uint32_t dir_ino, uint32_t ino, const char *name, uint8_t ftype);
void dir_remove_entry(uint32_t dir_ino, const char *name);

/* ========== file.c 函数声明 ========== */
int  file_create(const char *name, uint32_t parent_ino, uint16_t perm);
int  file_delete(const char *name, uint32_t parent_ino);
int  file_open(const char *name, uint16_t mode);
int  file_close(int fd);
int  file_read(int fd, char *buf, int size);
int  file_write(int fd, const char *buf, int size);
int  file_chmod(const char *name, uint32_t parent_ino, uint16_t new_perm);

/* ========== shell.c 函数声明 ========== */
void shell_run(void);

/* ========== trash.c 函数声明 ========== */
int  trash_move(const char *path);
int  trash_list(void);
int  trash_restore(const char *name);
int  trash_empty(void);

/* ========== mmap.c 函数声明 ========== */
void* fs_mmap(int fd, size_t length);
int  fs_munmap(void *addr, size_t length);
int  fs_msync(void *addr);
int  mmap_list(void);

/* ========== journal.c 函数声明 ========== */
void journal_init(void);
void journal_recover(void);
void journal_log_create(uint32_t inode_no);
void journal_log_delete(uint32_t inode_no);
void journal_log_write(uint32_t inode_no, uint32_t block_no, const void *data, size_t len);
void journal_log_chmod(uint32_t inode_no, uint16_t new_perm);
void journal_log_link(uint32_t inode_no, uint32_t parent_inode);
void journal_list(void);
void journal_clear(void);
void journal_toggle(int enable);

/* 工具函数 */
time_t get_current_time(void);

#endif /* _FS_H_ */
