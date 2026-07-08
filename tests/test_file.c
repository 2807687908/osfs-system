#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include "../fs.h"

static void test_file_create_delete(void) {
    int ret = file_create("test_file.txt", 0644);
    CU_ASSERT(ret == 0);
    
    uint32_t ino = dir_lookup(1, "test_file.txt");
    CU_ASSERT(ino != 0);
    
    ret = file_delete("test_file.txt");
    CU_ASSERT(ret == 0);
    
    ino = dir_lookup(1, "test_file.txt");
    CU_ASSERT(ino == 0);
}

static void test_file_open_close(void) {
    file_create("test_open.txt", 0644);
    
    int fd = file_open("test_open.txt", OPEN_READ);
    CU_ASSERT(fd >= 0);
    
    int ret = file_close(fd);
    CU_ASSERT(ret == 0);
    
    file_delete("test_open.txt");
}

static void test_file_read_write(void) {
    file_create("test_rw.txt", 0644);
    
    int fd = file_open("test_rw.txt", OPEN_WRITE);
    CU_ASSERT(fd >= 0);
    
    const char *data = "Hello, World!";
    int ret = file_write(fd, data, strlen(data));
    CU_ASSERT(ret == (int)strlen(data));
    
    file_close(fd);
    
    fd = file_open("test_rw.txt", OPEN_READ);
    CU_ASSERT(fd >= 0);
    
    char buf[100];
    ret = file_read(fd, buf, 100);
    CU_ASSERT(ret == (int)strlen(data));
    CU_ASSERT(strcmp(buf, data) == 0);
    
    file_close(fd);
    file_delete("test_rw.txt");
}

int main() {
    CU_initialize_registry();
    
    CU_pSuite suite = CU_add_suite("File Tests", NULL, NULL);
    
    CU_add_test(suite, "file_create_delete", test_file_create_delete);
    CU_add_test(suite, "file_open_close", test_file_open_close);
    CU_add_test(suite, "file_read_write", test_file_read_write);
    
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    
    return 0;
}