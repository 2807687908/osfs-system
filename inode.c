#include "fs.h"

/* 每个间接块可以存储的块号数 */
#define ADDRS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))  /* 256 */

/* 直接块数量 */
#define DIRECT_BLOCKS 12

static uint32_t indir_buf[ADDRS_PER_BLOCK];  /* 间接块缓冲 */

/*
 * 读取指定inode
 */
void read_inode(uint32_t ino, struct inode *ip)
{
    uint32_t block_no, offset;
    char buf[BLOCK_SIZE];

    if (ino >= TOTAL_INODES) {
        fprintf(stderr, "[ERROR] read_inode: inode号 %u 超出范围\n", ino);
        memset(ip, 0, sizeof(*ip));
        return;
    }

    block_no = INODE_TABLE_START + ino / INODES_PER_BLOCK;
    offset   = (ino % INODES_PER_BLOCK) * INODE_SIZE;

    read_block(block_no, buf);
    memcpy(ip, buf + offset, INODE_SIZE);
}

/*
 * 写入指定inode
 */
void write_inode(uint32_t ino, const struct inode *ip)
{
    uint32_t block_no, offset;
    char buf[BLOCK_SIZE];

    if (ino >= TOTAL_INODES) {
        fprintf(stderr, "[ERROR] write_inode: inode号 %u 超出范围\n", ino);
        return;
    }

    block_no = INODE_TABLE_START + ino / INODES_PER_BLOCK;
    offset   = (ino % INODES_PER_BLOCK) * INODE_SIZE;

    read_block(block_no, buf);
    memcpy(buf + offset, ip, INODE_SIZE);
    write_block(block_no, buf);
}

/*
 * 获取inode中指定偏移量对应的物理块号
 * offset: 文件内字节偏移
 * alloc: 如果块不存在是否分配
 * 返回块号，失败返回0
 */
uint32_t get_block_by_offset(uint32_t ino, uint32_t offset, int alloc)
{
    struct inode ip;
    uint32_t block_idx;        /* 文件内的逻辑块号 */
    uint32_t dblk_idx;         /* 在二级间接块中的一级块索引 */
    uint32_t dblk_indirect_idx;/* 在二级间接块中的二级索引 */

    read_inode(ino, &ip);
    block_idx = offset / BLOCK_SIZE;

    /* 情况1: 直接块 */
    if (block_idx < DIRECT_BLOCKS) {
        if (ip.i_block[block_idx] == 0 && alloc) {
            int new_blk = block_alloc();
            if (new_blk < 0) return 0;
            ip.i_block[block_idx] = (uint32_t)new_blk;
            ip.i_blocks++;
            write_inode(ino, &ip);
        }
        return ip.i_block[block_idx];
    }
    block_idx -= DIRECT_BLOCKS;

    /* 情况2: 一级间接块 */
    if (block_idx < ADDRS_PER_BLOCK) {
        if (ip.i_block[12] == 0) {
            if (!alloc) return 0;
            {
                int new_blk = block_alloc();
                if (new_blk < 0) return 0;
                ip.i_block[12] = (uint32_t)new_blk;
                ip.i_blocks++;
                write_inode(ino, &ip);
            }
        }
        /* 读取间接块 */
        read_block(ip.i_block[12], indir_buf);

        if (indir_buf[block_idx] == 0 && alloc) {
            int new_blk = block_alloc();
            if (new_blk < 0) return 0;
            indir_buf[block_idx] = (uint32_t)new_blk;
            ip.i_blocks++;
            write_inode(ino, &ip);
            write_block(ip.i_block[12], indir_buf);
        }
        return indir_buf[block_idx];
    }
    block_idx -= ADDRS_PER_BLOCK;

    /* 情况3: 二级间接块 */
    if (block_idx < ADDRS_PER_BLOCK * ADDRS_PER_BLOCK) {
        dblk_idx = block_idx / ADDRS_PER_BLOCK;
        dblk_indirect_idx = block_idx % ADDRS_PER_BLOCK;

        /* 分配/获取二级间接块 */
        if (ip.i_block[13] == 0) {
            if (!alloc) return 0;
            {
                int new_blk = block_alloc();
                if (new_blk < 0) return 0;
                ip.i_block[13] = (uint32_t)new_blk;
                ip.i_blocks++;
                write_inode(ino, &ip);
            }
        }

        /* 读取二级间接块 */
        read_block(ip.i_block[13], indir_buf);
        if (indir_buf[dblk_idx] == 0) {
            if (!alloc) return 0;
            {
                int new_blk = block_alloc();
                if (new_blk < 0) return 0;
                indir_buf[dblk_idx] = (uint32_t)new_blk;
                ip.i_blocks++;
                write_inode(ino, &ip);
                write_block(ip.i_block[13], indir_buf);
            }
        }

        /* 读取一级间接块 */
        read_block(indir_buf[dblk_idx], indir_buf);
        if (indir_buf[dblk_indirect_idx] == 0 && alloc) {
            int new_blk = block_alloc();
            if (new_blk < 0) return 0;
            indir_buf[dblk_indirect_idx] = (uint32_t)new_blk;
            ip.i_blocks++;
            write_inode(ino, &ip);
            {
                /* 需要重新读取二级间接块以找到一级间接块的位置 */
                uint32_t tmp[ADDRS_PER_BLOCK];
                read_block(ip.i_block[13], tmp);
                write_block(tmp[dblk_idx], indir_buf);
            }
        }
        return indir_buf[dblk_indirect_idx];
    }
    block_idx -= ADDRS_PER_BLOCK * ADDRS_PER_BLOCK;

    /* 情况4: 三级间接块 */
    {
        uint32_t t1, t2, t3;
        t1 = block_idx / (ADDRS_PER_BLOCK * ADDRS_PER_BLOCK);
        t2 = (block_idx / ADDRS_PER_BLOCK) % ADDRS_PER_BLOCK;
        t3 = block_idx % ADDRS_PER_BLOCK;

        if (ip.i_block[14] == 0) {
            if (!alloc) return 0;
            {
                int new_blk = block_alloc();
                if (new_blk < 0) return 0;
                ip.i_block[14] = (uint32_t)new_blk;
                ip.i_blocks++;
                write_inode(ino, &ip);
            }
        }

        read_block(ip.i_block[14], indir_buf);
        if (indir_buf[t1] == 0) {
            if (!alloc) return 0;
            {
                int new_blk = block_alloc();
                if (new_blk < 0) return 0;
                indir_buf[t1] = (uint32_t)new_blk;
                ip.i_blocks++;
                write_inode(ino, &ip);
                write_block(ip.i_block[14], indir_buf);
            }
        }

        read_block(indir_buf[t1], indir_buf);
        if (indir_buf[t2] == 0) {
            if (!alloc) return 0;
            {
                int new_blk = block_alloc();
                if (new_blk < 0) return 0;
                indir_buf[t2] = (uint32_t)new_blk;
                ip.i_blocks++;
                write_inode(ino, &ip);
                /* 找到t1块重新写入 */
                {
                    uint32_t tmp[ADDRS_PER_BLOCK];
                    read_block(ip.i_block[14], tmp);
                    write_block(tmp[t1], indir_buf);
                }
            }
        }

        read_block(indir_buf[t2], indir_buf);
        if (indir_buf[t3] == 0 && alloc) {
            int new_blk = block_alloc();
            if (new_blk < 0) return 0;
            indir_buf[t3] = (uint32_t)new_blk;
            ip.i_blocks++;
            write_inode(ino, &ip);
            {
                uint32_t tmp[ADDRS_PER_BLOCK];
                read_block(ip.i_block[14], tmp);
                {
                    uint32_t tmp2[ADDRS_PER_BLOCK];
                    read_block(tmp[t1], tmp2);
                    write_block(tmp2[t2], indir_buf);
                }
            }
        }
        return indir_buf[t3];
    }
}

/*
 * 释放间接块中的所有数据块
 */
static void free_indirect_blocks(uint32_t indirect_blk, int level)
{
    uint32_t i;
    if (indirect_blk == 0) return;

    read_block(indirect_blk, indir_buf);

    if (level == 1) {
        /* 一级间接块: 直接释放所有指向的数据块 */
        for (i = 0; i < ADDRS_PER_BLOCK; i++) {
            if (indir_buf[i] != 0) {
                block_free(indir_buf[i]);
            }
        }
    } else {
        /* 多级: 递归释放 */
        for (i = 0; i < ADDRS_PER_BLOCK; i++) {
            if (indir_buf[i] != 0) {
                free_indirect_blocks(indir_buf[i], level - 1);
                block_free(indir_buf[i]);
            }
        }
    }
}

/*
 * 截断文件到指定大小，释放多余的数据块
 */
int inode_truncate(struct inode *ip, uint32_t new_size)
{
    uint32_t old_blocks_needed, new_blocks_needed;
    uint32_t i;
    uint32_t total_freed = 0;

    old_blocks_needed = (ip->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    new_blocks_needed = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (new_blocks_needed >= old_blocks_needed) {
        /* 不需要截断 */
        ip->i_size = new_size;
        return 0;
    }

    /* 释放多余的直接块 */
    for (i = new_blocks_needed; i < old_blocks_needed && i < DIRECT_BLOCKS; i++) {
        if (ip->i_block[i] != 0) {
            block_free(ip->i_block[i]);
            ip->i_block[i] = 0;
            total_freed++;
        }
    }

    /* 释放一级间接块中的多余块 */
    if (old_blocks_needed > DIRECT_BLOCKS && ip->i_block[12] != 0) {
        uint32_t old_indirect = old_blocks_needed - DIRECT_BLOCKS;
        uint32_t new_indirect = (new_blocks_needed > DIRECT_BLOCKS)
            ? (new_blocks_needed - DIRECT_BLOCKS) : 0;

        if (old_indirect > ADDRS_PER_BLOCK) old_indirect = ADDRS_PER_BLOCK;
        if (new_indirect > ADDRS_PER_BLOCK) new_indirect = ADDRS_PER_BLOCK;

        read_block(ip->i_block[12], indir_buf);
        for (i = new_indirect; i < old_indirect; i++) {
            if (indir_buf[i] != 0) {
                block_free(indir_buf[i]);
                indir_buf[i] = 0;
                total_freed++;
            }
        }

        if (new_indirect == 0) {
            /* 一级间接块本身也不再需要 */
            block_free(ip->i_block[12]);
            ip->i_block[12] = 0;
            total_freed++;
        } else {
            write_block(ip->i_block[12], indir_buf);
        }
    }

    /* 二级间接块: 如果新大小不需要，整棵释放 */
    if (old_blocks_needed > DIRECT_BLOCKS + ADDRS_PER_BLOCK && ip->i_block[13] != 0) {
        if (new_blocks_needed <= DIRECT_BLOCKS + ADDRS_PER_BLOCK) {
            free_indirect_blocks(ip->i_block[13], 2);
            block_free(ip->i_block[13]);
            ip->i_block[13] = 0;
            total_freed++;
        }
    }

    /* 三级间接块 */
    if (old_blocks_needed > DIRECT_BLOCKS + ADDRS_PER_BLOCK + ADDRS_PER_BLOCK * ADDRS_PER_BLOCK
        && ip->i_block[14] != 0) {
        if (new_blocks_needed <= DIRECT_BLOCKS + ADDRS_PER_BLOCK + ADDRS_PER_BLOCK * ADDRS_PER_BLOCK) {
            free_indirect_blocks(ip->i_block[14], 3);
            block_free(ip->i_block[14]);
            ip->i_block[14] = 0;
            total_freed++;
        }
    }

    ip->i_blocks -= total_freed;
    ip->i_size = new_size;
    ip->i_mtime = (uint32_t)time(NULL);
    return 0;
}
