#include "fs.h"

/* ========== 全局变量 ========== */
FILE *disk = NULL;
struct superblock sb;
struct group_desc gd;
int current_uid = -1;
char current_user[MAX_USERNAME] = "";
uint32_t current_dir = 1;
char current_path[MAX_PATH] = "/";
struct file_desc fd_table[MAX_OPEN_FILES];

/* 临时缓冲区 */
static char block_buf[BLOCK_SIZE];
static char zero_buf[BLOCK_SIZE] = {0};

/* ========== 磁盘初始化/格式化 ========== */
void disk_init(void)
{
    const char *disk_path = "disk.img";
    int i;

    /* 尝试打开已有磁盘 */
    disk = fopen(disk_path, "rb+");
    if (disk) {
        sb_read();
        if (sb.s_magic == EXT2_MAGIC) {
            printf("[INFO] 已加载现有文件系统\n");
            /* 初始化fd表 */
            for (i = 0; i < MAX_OPEN_FILES; i++)
                fd_table[i].used = 0;
            return;
        }
        fclose(disk);
        printf("[WARN] 磁盘文件损坏，重新格式化\n");
    }

    /* 创建新磁盘文件 */
    disk = fopen(disk_path, "wb+");
    if (!disk) {
        fprintf(stderr, "[ERROR] 无法创建磁盘文件 %s\n", disk_path);
        exit(1);
    }

    printf("[INFO] 正在格式化文件系统...\n");

    /* 填零整个磁盘 */
    memset(block_buf, 0, BLOCK_SIZE);
    for (i = 0; i < TOTAL_BLOCKS; i++)
        fwrite(block_buf, BLOCK_SIZE, 1, disk);
    fflush(disk);

    /* 初始化超级块 */
    memset(&sb, 0, sizeof(sb));
    sb.s_inodes_count      = TOTAL_INODES;
    sb.s_blocks_count      = TOTAL_BLOCKS;
    sb.s_free_blocks_count = TOTAL_BLOCKS - DATA_START_BLOCK;
    sb.s_free_inodes_count = TOTAL_INODES - 2;  /* 保留0号, 根目录用1号 */
    sb.s_first_data_block  = DATA_START_BLOCK;
    sb.s_log_block_size    = 0;   /* 1024=2^(10+0) */
    sb.s_magic             = EXT2_MAGIC;
    sb.s_root_ino          = 1;
    sb.s_user_count        = 2;
    sb.s_user_table_start  = USER_TABLE_START;

    /* 初始化组描述符 */
    memset(&gd, 0, sizeof(gd));
    gd.bg_block_bitmap      = BLOCK_BITMAP_START;
    gd.bg_inode_bitmap      = INODE_BITMAP_START;
    gd.bg_inode_table       = INODE_TABLE_START;
    gd.bg_free_blocks_count = sb.s_free_blocks_count;
    gd.bg_free_inodes_count = sb.s_free_inodes_count;

    /* 块位图: 标记系统块已用 (0 ~ DATA_START_BLOCK-1) */
    for (i = 0; i < DATA_START_BLOCK; i++)
        bitmap_set(BLOCK_BITMAP_START, i, 1);

    /* inode位图: 标记保留inode */
    bitmap_set(INODE_BITMAP_START, 0, 1);  /* inode 0 保留 */
    bitmap_set(INODE_BITMAP_START, 1, 1);  /* inode 1 根目录 */

    /* 创建根目录inode */
    {
        struct inode root;
        time_t now = time(NULL);
        uint32_t root_block;
        char buf[BLOCK_SIZE];
        struct dir_entry *de;
        int dot_sz, dotdot_sz;

        memset(&root, 0, sizeof(root));
        root.i_mode = (FT_DIRECTORY << 12) | DEFAULT_DIR_PERM;
        root.i_uid  = 0;
        root.i_links_count = 2;
        root.i_blocks = 1;

        root_block = (uint32_t)block_alloc();
        root.i_block[0] = root_block;

        memset(buf, 0, BLOCK_SIZE);

        /* . 条目 */
        de = (struct dir_entry *)buf;
        de->inode = 1;
        dot_sz = (int)(sizeof(struct dir_entry) + 1);
        dot_sz = (dot_sz + 3) & ~3;  /* 4字节对齐 */
        de->rec_len = (uint16_t)dot_sz;
        de->name_len = 1;
        de->file_type = FT_DIRECTORY;
        *(char *)(de + 1) = '.';

        /* .. 条目 */
        dotdot_sz = (int)(sizeof(struct dir_entry) + 2);
        dotdot_sz = (dotdot_sz + 3) & ~3;
        de = (struct dir_entry *)(buf + dot_sz);
        de->inode = 1;
        de->rec_len = (uint16_t)dotdot_sz;
        de->name_len = 2;
        de->file_type = FT_DIRECTORY;
        memcpy((char *)(de + 1), "..", 2);

        root.i_size = dot_sz + dotdot_sz;
        root.i_atime = root.i_ctime = root.i_mtime = (uint32_t)now;

        write_block(root_block, buf);
        write_inode(1, &root);
    }

    /* 写入默认用户 */
    {
        struct user_entry users[MAX_USERS];
        memset(users, 0, sizeof(users));
        strcpy(users[0].name, "root");
        strcpy(users[0].passwd, "root");
        users[0].uid = 0;
        strcpy(users[1].name, "user1");
        strcpy(users[1].passwd, "123");
        users[1].uid = 1;
        /* 写入用户表块 */
        write_block(USER_TABLE_START, users);
        write_block(USER_TABLE_START + 1, ((char *)users) + BLOCK_SIZE);
    }

    /* 写入超级块和组描述符到磁盘 */
    sb_write();

    /* 初始化文件描述符表 */
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        fd_table[i].used = 0;
        fd_table[i].inode_no = 0;
        fd_table[i].offset = 0;
        fd_table[i].mode = 0;
    }

    printf("[INFO] 文件系统格式化完成\n");
    printf("  磁盘大小: 32 MB, 块大小: %d B\n", BLOCK_SIZE);
    printf("  总块数: %d, Inode数: %d\n", TOTAL_BLOCKS, TOTAL_INODES);
    printf("  数据区起始块: %d, 可用数据块: %d\n",
           DATA_START_BLOCK, sb.s_free_blocks_count);
    printf("  默认用户: root/root, user1/123\n");
}

void disk_close(void)
{
    if (disk) { fclose(disk); disk = NULL; }
}

/* ========== 超级块读写 ========== */
void sb_read(void)
{
    rewind(disk);
    fread(&sb, sizeof(sb), 1, disk);
    fread(&gd, sizeof(gd), 1, disk);
}

void sb_write(void)
{
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &sb, sizeof(sb));
    memcpy(buf + sizeof(sb), &gd, sizeof(gd));
    write_block(SUPERBLOCK_BLOCK, buf);
}

/* ========== 用户表读写 ========== */
void user_read_table(struct user_entry *users)
{
    char buf[BLOCK_SIZE * USER_TABLE_BLOCKS];
    read_block(USER_TABLE_START, buf);
    read_block(USER_TABLE_START + 1, buf + BLOCK_SIZE);
    memcpy(users, buf, sizeof(struct user_entry) * MAX_USERS);
}

void user_write_table(const struct user_entry *users)
{
    char buf[BLOCK_SIZE * USER_TABLE_BLOCKS];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, users, sizeof(struct user_entry) * MAX_USERS);
    write_block(USER_TABLE_START, buf);
    write_block(USER_TABLE_START + 1, buf + BLOCK_SIZE);
}

/* ========== 块读写 ========== */
void read_block(uint32_t block_no, void *buf)
{
    if (block_no >= TOTAL_BLOCKS) {
        fprintf(stderr, "[ERROR] read_block: 块号%u超出范围\n", block_no);
        return;
    }
    fseek(disk, (long)block_no * BLOCK_SIZE, SEEK_SET);
    if (fread(buf, BLOCK_SIZE, 1, disk) != 1) {
        fprintf(stderr, "[ERROR] read_block: 读取块%u失败\n", block_no);
    }
}

void write_block(uint32_t block_no, const void *buf)
{
    if (block_no >= TOTAL_BLOCKS) {
        fprintf(stderr, "[ERROR] write_block: 块号%u超出范围\n", block_no);
        return;
    }
    fseek(disk, (long)block_no * BLOCK_SIZE, SEEK_SET);
    fwrite(buf, BLOCK_SIZE, 1, disk);
    fflush(disk);
}

/* ========== 位图操作 ========== */
void bitmap_set(uint32_t start_block, uint32_t bit_no, int val)
{
    uint32_t byte_offset = bit_no / 8;
    uint32_t bit_offset  = bit_no % 8;
    uint32_t block_no = start_block + byte_offset / BLOCK_SIZE;
    uint32_t offset   = byte_offset % BLOCK_SIZE;

    read_block(block_no, block_buf);
    if (val) {
        block_buf[offset] |= (1U << bit_offset);
    } else {
        block_buf[offset] &= ~(1U << bit_offset);
    }
    write_block(block_no, block_buf);
}

int bitmap_get(uint32_t start_block, uint32_t bit_no)
{
    uint32_t byte_offset = bit_no / 8;
    uint32_t bit_offset  = bit_no % 8;
    uint32_t block_no = start_block + byte_offset / BLOCK_SIZE;
    uint32_t offset   = byte_offset % BLOCK_SIZE;

    read_block(block_no, block_buf);
    return (block_buf[offset] >> bit_offset) & 1;
}

/* ========== 块分配/释放 ========== */
int block_alloc(void)
{
    uint32_t i;
    if (sb.s_free_blocks_count == 0) {
        fprintf(stderr, "[ERROR] 没有空闲块\n");
        return -1;
    }
    for (i = DATA_START_BLOCK; i < TOTAL_BLOCKS; i++) {
        if (bitmap_get(BLOCK_BITMAP_START, i) == 0) {
            bitmap_set(BLOCK_BITMAP_START, i, 1);
            sb.s_free_blocks_count--;
            gd.bg_free_blocks_count--;
            sb_write();
            write_block(i, zero_buf);  /* 清零新块 */
            return (int)i;
        }
    }
    fprintf(stderr, "[ERROR] 位图不一致\n");
    return -1;
}

void block_free(uint32_t block)
{
    if (block < DATA_START_BLOCK) {
        fprintf(stderr, "[ERROR] 不能释放系统块%u\n", block);
        return;
    }
    if (block >= TOTAL_BLOCKS) {
        fprintf(stderr, "[ERROR] 块号%u超出范围\n", block);
        return;
    }
    if (bitmap_get(BLOCK_BITMAP_START, block) == 0) {
        fprintf(stderr, "[WARN] 块%u已经空闲\n", block);
        return;
    }
    bitmap_set(BLOCK_BITMAP_START, block, 0);
    sb.s_free_blocks_count++;
    gd.bg_free_blocks_count++;
    sb_write();
}

/* ========== Inode分配/释放 ========== */
int inode_alloc(void)
{
    uint32_t i;
    if (sb.s_free_inodes_count == 0) {
        fprintf(stderr, "[ERROR] 没有空闲inode\n");
        return -1;
    }
    for (i = 1; i < TOTAL_INODES; i++) {
        if (bitmap_get(INODE_BITMAP_START, i) == 0) {
            bitmap_set(INODE_BITMAP_START, i, 1);
            sb.s_free_inodes_count--;
            gd.bg_free_inodes_count--;
            sb_write();
            return (int)i;
        }
    }
    fprintf(stderr, "[ERROR] inode位图不一致\n");
    return -1;
}

void inode_free(uint32_t ino)
{
    struct inode ip;
    if (ino < 1 || ino >= TOTAL_INODES) {
        fprintf(stderr, "[ERROR] inode号%u超出范围\n", ino);
        return;
    }
    if (bitmap_get(INODE_BITMAP_START, ino) == 0) {
        fprintf(stderr, "[WARN] inode%u已经空闲\n", ino);
        return;
    }
    read_inode(ino, &ip);
    inode_truncate(&ip, 0);  /* 先释放所有数据块 */

    bitmap_set(INODE_BITMAP_START, ino, 0);
    sb.s_free_inodes_count++;
    gd.bg_free_inodes_count++;
    sb_write();
}

time_t get_current_time(void)
{
    return time(NULL);
}

block_cache_entry block_cache[BLOCK_CACHE_SIZE];
int cache_hits = 0;
int cache_misses = 0;

static int cache_find(uint32_t block_no) {
    int i;
    for (i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].block_no == block_no) {
            return i;
        }
    }
    return -1;
}

static int cache_replace(void) {
    static int next = 0;
    int victim = next;
    next = (next + 1) % BLOCK_CACHE_SIZE;
    return victim;
}

void cache_read_block(uint32_t block_no, char *buf) {
    int idx = cache_find(block_no);
    if (idx >= 0) {
        cache_hits++;
        memcpy(buf, block_cache[idx].data, BLOCK_SIZE);
        return;
    }
    cache_misses++;
    read_block(block_no, buf);
    idx = cache_replace();
    if (block_cache[idx].dirty) {
        write_block(block_cache[idx].block_no, block_cache[idx].data);
    }
    block_cache[idx].block_no = block_no;
    memcpy(block_cache[idx].data, buf, BLOCK_SIZE);
    block_cache[idx].dirty = 0;
}

void cache_write_block(uint32_t block_no, const char *buf) {
    int idx = cache_find(block_no);
    if (idx >= 0) {
        cache_hits++;
        memcpy(block_cache[idx].data, buf, BLOCK_SIZE);
        block_cache[idx].dirty = 1;
        return;
    }
    cache_misses++;
    idx = cache_replace();
    if (block_cache[idx].dirty) {
        write_block(block_cache[idx].block_no, block_cache[idx].data);
    }
    block_cache[idx].block_no = block_no;
    memcpy(block_cache[idx].data, buf, BLOCK_SIZE);
    block_cache[idx].dirty = 1;
}

void cache_flush(void) {
    int i;
    for (i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].dirty && block_cache[i].block_no != 0) {
            write_block(block_cache[i].block_no, block_cache[i].data);
            block_cache[i].dirty = 0;
        }
    }
}

void cache_clear(void) {
    int i;
    for (i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].dirty && block_cache[i].block_no != 0) {
            write_block(block_cache[i].block_no, block_cache[i].data);
        }
        block_cache[i].block_no = 0;
        block_cache[i].dirty = 0;
    }
    cache_hits = 0;
    cache_misses = 0;
}
