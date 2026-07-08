#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include "../fs.h"

static void test_dir_create_remove(void) {
    uint32_t parent_ino = 1;
    uint32_t dir_ino = dir_create("test_dir", parent_ino);
    CU_ASSERT(dir_ino != 0);
    
    uint32_t found_ino = dir_lookup(parent_ino, "test_dir");
    CU_ASSERT(found_ino == dir_ino);
    
    int ret = dir_remove("test_dir", parent_ino);
    CU_ASSERT(ret == 0);
    
    found_ino = dir_lookup(parent_ino, "test_dir");
    CU_ASSERT(found_ino == 0);
}

static void test_dir_add_entry(void) {
    uint32_t dir_ino = dir_create("test_dir2", 1);
    CU_ASSERT(dir_ino != 0);
    
    uint32_t file_ino = inode_alloc();
    CU_ASSERT(file_ino != 0);
    
    dir_add_entry(dir_ino, file_ino, "test_file", FT_REG_FILE);
    
    uint32_t found_ino = dir_lookup(dir_ino, "test_file");
    CU_ASSERT(found_ino == file_ino);
    
    dir_remove("test_dir2", 1);
    inode_free(file_ino);
}

static void test_dir_list(void) {
    uint32_t dir_ino = dir_create("test_dir3", 1);
    CU_ASSERT(dir_ino != 0);
    
    int count = dir_list(dir_ino);
    CU_ASSERT(count >= 0);
    
    dir_remove("test_dir3", 1);
}

int main() {
    CU_initialize_registry();
    
    CU_pSuite suite = CU_add_suite("Directory Tests", NULL, NULL);
    
    CU_add_test(suite, "dir_create_remove", test_dir_create_remove);
    CU_add_test(suite, "dir_add_entry", test_dir_add_entry);
    CU_add_test(suite, "dir_list", test_dir_list);
    
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    
    return 0;
}