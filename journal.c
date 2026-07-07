#include "fs.h"

#define JOURNAL_BLOCK_START 138
#define JOURNAL_BLOCKS 16
#define JOURNAL_ENTRY_SIZE 64

#define JOURNAL_MAGIC 0x4A4E5452

struct journal_entry {
    uint32_t magic;
    uint32_t type;
    uint32_t inode_no;
    uint32_t block_no;
    uint32_t timestamp;
    uint16_t data_len;
    uint8_t data[44];
};

#define JOURNAL_CREATE 1
#define JOURNAL_DELETE 2
#define JOURNAL_WRITE 3
#define JOURNAL_CHMOD 4
#define JOURNAL_LINK 5

static uint32_t journal_head = 0;
static uint32_t journal_tail = 0;
static int journal_enabled = 1;

static void journal_write_entry(struct journal_entry *entry)
{
    char buf[BLOCK_SIZE];
    uint32_t block_no = JOURNAL_BLOCK_START + (journal_head / (BLOCK_SIZE / JOURNAL_ENTRY_SIZE));
    uint32_t offset = (journal_head % (BLOCK_SIZE / JOURNAL_ENTRY_SIZE)) * JOURNAL_ENTRY_SIZE;
    
    entry->magic = JOURNAL_MAGIC;
    entry->timestamp = (uint32_t)time(NULL);
    
    read_block(block_no, buf);
    memcpy(buf + offset, entry, JOURNAL_ENTRY_SIZE);
    write_block(block_no, buf);
    
    journal_head++;
    if (journal_head >= JOURNAL_BLOCKS * (BLOCK_SIZE / JOURNAL_ENTRY_SIZE)) {
        journal_head = 0;
    }
    
    if (journal_head == journal_tail) {
        journal_tail++;
        if (journal_tail >= JOURNAL_BLOCKS * (BLOCK_SIZE / JOURNAL_ENTRY_SIZE)) {
            journal_tail = 0;
        }
    }
}

void journal_log_create(uint32_t inode_no)
{
    if (!journal_enabled) return;
    
    struct journal_entry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = JOURNAL_CREATE;
    entry.inode_no = inode_no;
    
    journal_write_entry(&entry);
}

void journal_log_delete(uint32_t inode_no)
{
    if (!journal_enabled) return;
    
    struct journal_entry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = JOURNAL_DELETE;
    entry.inode_no = inode_no;
    
    journal_write_entry(&entry);
}

void journal_log_write(uint32_t inode_no, uint32_t block_no, const void *data, size_t len)
{
    if (!journal_enabled) return;
    
    struct journal_entry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = JOURNAL_WRITE;
    entry.inode_no = inode_no;
    entry.block_no = block_no;
    entry.data_len = (uint16_t)(len < sizeof(entry.data) ? len : sizeof(entry.data));
    memcpy(entry.data, data, entry.data_len);
    
    journal_write_entry(&entry);
}

void journal_log_chmod(uint32_t inode_no, uint16_t new_perm)
{
    if (!journal_enabled) return;
    
    struct journal_entry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = JOURNAL_CHMOD;
    entry.inode_no = inode_no;
    entry.data_len = 2;
    memcpy(entry.data, &new_perm, 2);
    
    journal_write_entry(&entry);
}

void journal_log_link(uint32_t inode_no, uint32_t parent_inode)
{
    if (!journal_enabled) return;
    
    struct journal_entry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = JOURNAL_LINK;
    entry.inode_no = inode_no;
    entry.block_no = parent_inode;
    
    journal_write_entry(&entry);
}

void journal_init(void)
{
    char buf[BLOCK_SIZE];
    uint32_t i;
    
    for (i = JOURNAL_BLOCK_START; i < JOURNAL_BLOCK_START + JOURNAL_BLOCKS; i++) {
        read_block(i, buf);
        if (buf[0] != 0) {
            break;
        }
    }
    
    if (i == JOURNAL_BLOCK_START + JOURNAL_BLOCKS) {
        for (i = JOURNAL_BLOCK_START; i < JOURNAL_BLOCK_START + JOURNAL_BLOCKS; i++) {
            memset(buf, 0, BLOCK_SIZE);
            write_block(i, buf);
        }
        journal_head = 0;
        journal_tail = 0;
        printf("[INFO] 日志系统初始化完成\n");
    } else {
        journal_recover();
    }
}

void journal_recover(void)
{
    char buf[BLOCK_SIZE];
    struct journal_entry entry;
    uint32_t i, count = 0;
    
    printf("[INFO] 正在恢复文件系统日志...\n");
    
    i = journal_tail;
    while (i != journal_head) {
        uint32_t block_no = JOURNAL_BLOCK_START + (i / (BLOCK_SIZE / JOURNAL_ENTRY_SIZE));
        uint32_t offset = (i % (BLOCK_SIZE / JOURNAL_ENTRY_SIZE)) * JOURNAL_ENTRY_SIZE;
        
        read_block(block_no, buf);
        memcpy(&entry, buf + offset, JOURNAL_ENTRY_SIZE);
        
        if (entry.magic != JOURNAL_MAGIC) {
            break;
        }
        
        switch (entry.type) {
            case JOURNAL_CREATE:
                printf("  [RECOVER] 创建 inode=%u\n", entry.inode_no);
                break;
            case JOURNAL_DELETE:
                printf("  [RECOVER] 删除 inode=%u\n", entry.inode_no);
                break;
            case JOURNAL_WRITE:
                printf("  [RECOVER] 写入 inode=%u, block=%u\n", entry.inode_no, entry.block_no);
                break;
            case JOURNAL_CHMOD:
                printf("  [RECOVER] 权限修改 inode=%u\n", entry.inode_no);
                break;
            case JOURNAL_LINK:
                printf("  [RECOVER] 链接 inode=%u -> parent=%u\n", entry.inode_no, entry.block_no);
                break;
        }
        
        count++;
        i++;
        if (i >= JOURNAL_BLOCKS * (BLOCK_SIZE / JOURNAL_ENTRY_SIZE)) {
            i = 0;
        }
    }
    
    printf("[INFO] 日志恢复完成，共处理 %u 条记录\n", count);
}

void journal_list(void)
{
    char buf[BLOCK_SIZE];
    struct journal_entry entry;
    uint32_t i, count = 0;
    
    printf("文件系统日志:\n");
    printf("%-16s %-12s %-8s %-10s %s\n", "时间", "类型", "Inode", "Block", "数据");
    printf("-------------------------------------------------------------------\n");
    
    i = journal_tail;
    while (i != journal_head) {
        uint32_t block_no = JOURNAL_BLOCK_START + (i / (BLOCK_SIZE / JOURNAL_ENTRY_SIZE));
        uint32_t offset = (i % (BLOCK_SIZE / JOURNAL_ENTRY_SIZE)) * JOURNAL_ENTRY_SIZE;
        
        read_block(block_no, buf);
        memcpy(&entry, buf + offset, JOURNAL_ENTRY_SIZE);
        
        if (entry.magic != JOURNAL_MAGIC) {
            break;
        }
        
        char time_str[20];
        struct tm *t = localtime((time_t *)&entry.timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
        
        const char *type_str;
        switch (entry.type) {
            case JOURNAL_CREATE: type_str = "CREATE"; break;
            case JOURNAL_DELETE: type_str = "DELETE"; break;
            case JOURNAL_WRITE:  type_str = "WRITE";  break;
            case JOURNAL_CHMOD:  type_str = "CHMOD";  break;
            case JOURNAL_LINK:   type_str = "LINK";   break;
            default:             type_str = "UNKNOWN";
        }
        
        printf("%-16s %-12s %-8u %-10u ", time_str, type_str, entry.inode_no, entry.block_no);
        
        if (entry.data_len > 0) {
            for (uint16_t j = 0; j < entry.data_len && j < 16; j++) {
                printf("%02x ", entry.data[j]);
            }
        }
        printf("\n");
        
        count++;
        i++;
        if (i >= JOURNAL_BLOCKS * (BLOCK_SIZE / JOURNAL_ENTRY_SIZE)) {
            i = 0;
        }
    }
    
    if (count == 0) {
        printf("(空)\n");
    }
    printf("共 %u 条记录\n", count);
}

void journal_clear(void)
{
    char buf[BLOCK_SIZE];
    uint32_t i;
    
    for (i = JOURNAL_BLOCK_START; i < JOURNAL_BLOCK_START + JOURNAL_BLOCKS; i++) {
        memset(buf, 0, BLOCK_SIZE);
        write_block(i, buf);
    }
    
    journal_head = 0;
    journal_tail = 0;
    printf("日志已清空\n");
}

void journal_toggle(int enable)
{
    journal_enabled = enable;
    printf("日志功能已%s\n", enable ? "启用" : "禁用");
}