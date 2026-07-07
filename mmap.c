#include "fs.h"

#define MAX_MMAP_REGIONS 16

struct mmap_region {
    void *addr;
    uint32_t inode_no;
    size_t length;
    int mode;
    uint8_t used;
};

static struct mmap_region mmap_table[MAX_MMAP_REGIONS];

void* fs_mmap(int fd, size_t length)
{
    struct inode ip;
    uint32_t ino;
    void *addr;
    int i;
    
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].used) {
        printf("无效的文件描述符: %d\n", fd);
        return NULL;
    }
    
    ino = fd_table[fd].inode_no;
    read_inode(ino, &ip);
    
    if (length == 0) {
        length = ip.i_size;
    }
    
    for (i = 0; i < MAX_MMAP_REGIONS; i++) {
        if (!mmap_table[i].used) break;
    }
    if (i >= MAX_MMAP_REGIONS) {
        printf("mmap区域已达上限(%d)\n", MAX_MMAP_REGIONS);
        return NULL;
    }
    
    addr = malloc(length);
    if (addr == NULL) {
        printf("内存分配失败\n");
        return NULL;
    }
    
    memset(addr, 0, length);
    
    int total_read = 0;
    char buf[BLOCK_SIZE];
    uint32_t offset = 0;
    
    while (total_read < (int)length && offset < ip.i_size) {
        uint32_t blk = offset / BLOCK_SIZE;
        uint32_t off = offset % BLOCK_SIZE;
        uint32_t phys = get_block_by_offset(ino, blk * BLOCK_SIZE, 0);
        
        if (phys == 0) break;
        
        read_block(phys, buf);
        
        int chunk = (int)(BLOCK_SIZE - off);
        if (chunk > (int)length - total_read) chunk = (int)length - total_read;
        if (chunk > (int)(ip.i_size - offset)) chunk = (int)(ip.i_size - offset);
        
        memcpy((char *)addr + total_read, buf + off, chunk);
        total_read += chunk;
        offset += chunk;
    }
    
    mmap_table[i].addr = addr;
    mmap_table[i].inode_no = ino;
    mmap_table[i].length = length;
    mmap_table[i].mode = fd_table[fd].mode;
    mmap_table[i].used = 1;
    
    printf("mmap成功: addr=%p, length=%zu, inode=%u\n", addr, length, ino);
    return addr;
}

int fs_munmap(void *addr, size_t length)
{
    int i;
    
    for (i = 0; i < MAX_MMAP_REGIONS; i++) {
        if (mmap_table[i].used && mmap_table[i].addr == addr) {
            break;
        }
    }
    
    if (i >= MAX_MMAP_REGIONS) {
        printf("未找到该mmap区域\n");
        return -1;
    }
    
    if (mmap_table[i].mode == OPEN_WRITE || mmap_table[i].mode == OPEN_RDWR) {
        struct inode ip;
        read_inode(mmap_table[i].inode_no, &ip);
        
        int total_written = 0;
        char buf[BLOCK_SIZE];
        uint32_t offset = 0;
        
        while (total_written < (int)length) {
            uint32_t blk = offset / BLOCK_SIZE;
            uint32_t off = offset % BLOCK_SIZE;
            uint32_t phys = get_block_by_offset(mmap_table[i].inode_no, blk * BLOCK_SIZE, 1);
            
            if (phys == 0) break;
            
            read_block(phys, buf);
            
            int chunk = (int)(BLOCK_SIZE - off);
            if (chunk > (int)length - total_written) chunk = (int)length - total_written;
            
            memcpy(buf + off, (char *)addr + total_written, chunk);
            write_block(phys, buf);
            
            total_written += chunk;
            offset += chunk;
        }
        
        if (offset > ip.i_size) {
            ip.i_size = offset;
        }
        ip.i_mtime = (uint32_t)time(NULL);
        write_inode(mmap_table[i].inode_no, &ip);
        
        printf("已将 %d 字节写回文件\n", total_written);
    }
    
    free(addr);
    mmap_table[i].used = 0;
    
    printf("munmap成功: addr=%p\n", addr);
    return 0;
}

int fs_msync(void *addr)
{
    int i;
    
    for (i = 0; i < MAX_MMAP_REGIONS; i++) {
        if (mmap_table[i].used && mmap_table[i].addr == addr) {
            break;
        }
    }
    
    if (i >= MAX_MMAP_REGIONS) {
        printf("未找到该mmap区域\n");
        return -1;
    }
    
    if (mmap_table[i].mode == OPEN_WRITE || mmap_table[i].mode == OPEN_RDWR) {
        struct inode ip;
        read_inode(mmap_table[i].inode_no, &ip);
        
        int total_written = 0;
        char buf[BLOCK_SIZE];
        uint32_t offset = 0;
        
        while (total_written < (int)mmap_table[i].length && offset < ip.i_size) {
            uint32_t blk = offset / BLOCK_SIZE;
            uint32_t off = offset % BLOCK_SIZE;
            uint32_t phys = get_block_by_offset(mmap_table[i].inode_no, blk * BLOCK_SIZE, 0);
            
            if (phys == 0) break;
            
            read_block(phys, buf);
            
            int chunk = (int)(BLOCK_SIZE - off);
            if (chunk > (int)mmap_table[i].length - total_written) 
                chunk = (int)mmap_table[i].length - total_written;
            if (chunk > (int)(ip.i_size - offset)) 
                chunk = (int)(ip.i_size - offset);
            
            memcpy(buf + off, (char *)mmap_table[i].addr + total_written, chunk);
            write_block(phys, buf);
            
            total_written += chunk;
            offset += chunk;
        }
        
        ip.i_mtime = (uint32_t)time(NULL);
        write_inode(mmap_table[i].inode_no, &ip);
        
        printf("msync成功: 已同步 %d 字节\n", total_written);
    }
    
    return 0;
}

int mmap_list(void)
{
    int i, count = 0;
    
    printf("mmap区域列表:\n");
    printf("  addr          inode    length    mode\n");
    printf("  ------------ -------- --------- ----\n");
    
    for (i = 0; i < MAX_MMAP_REGIONS; i++) {
        if (mmap_table[i].used) {
            printf("  %p  %-8u %-9zu %s\n",
                   mmap_table[i].addr,
                   mmap_table[i].inode_no,
                   mmap_table[i].length,
                   mmap_table[i].mode == OPEN_READ ? "r" :
                   (mmap_table[i].mode == OPEN_WRITE ? "w" : "rw"));
            count++;
        }
    }
    
    if (count == 0) {
        printf("  (无mmap区域)\n");
    }
    printf("共 %d 个\n", count);
    return 0;
}