#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include "../fs.h"

static void test_block_alloc_free(void) {
    uint32_t blk = block_alloc();
    CU_ASSERT(blk != 0);
    CU_ASSERT(blk >= DATA_START_BLOCK);
    CU_ASSERT(blk < sb.s_blocks_count);
    
    uint8_t used = block_is_used(blk);
    CU_ASSERT(used == 1);
    
    block_free(blk);
    used = block_is_used(blk);
    CU_ASSERT(used == 0);
}

static void test_inode_alloc_free(void) {
    uint32_t ino = inode_alloc();
    CU_ASSERT(ino != 0);
    CU_ASSERT(ino < TOTAL_INODES);
    
    struct inode ip;
    read_inode(ino, &ip);
    CU_ASSERT(ip.i_mode != 0);
    
    inode_free(ino);
    read_inode(ino, &ip);
    CU_ASSERT(ip.i_mode == 0);
}

static void test_sb_read_write(void) {
    sb.s_magic = EXT2_MAGIC;
    sb.s_blocks_count = 32768;
    sb.s_free_blocks_count = 32615;
    sb.s_inodes_count = 1024;
    sb.s_free_inodes_count = 1020;
    sb_write();
    
    struct super_block sb_test;
    struct group_desc gd_test;
    char buf[BLOCK_SIZE];
    read_block(SUPERBLOCK_BLOCK, buf);
    memcpy(&sb_test, buf, sizeof(sb_test));
    memcpy(&gd_test, buf + sizeof(sb_test), sizeof(gd_test));
    
    CU_ASSERT(sb_test.s_magic == EXT2_MAGIC);
    CU_ASSERT(sb_test.s_blocks_count == 32768);
    CU_ASSERT(sb_test.s_inodes_count == 1024);
}

int main() {
    CU_initialize_registry();
    
    CU_pSuite suite = CU_add_suite("File System Tests", NULL, NULL);
    
    CU_add_test(suite, "block_alloc_free", test_block_alloc_free);
    CU_add_test(suite, "inode_alloc_free", test_inode_alloc_free);
    CU_add_test(suite, "sb_read_write", test_sb_read_write);
    
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    
    return 0;
}