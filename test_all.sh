#!/bin/bash

echo "=============================================="
echo "  Linux二级文件系统 - 自动化测试脚本"
echo "  测试版本: v1"
echo "=============================================="

TEST_PASS=0
TEST_FAIL=0

run_test() {
    local test_name="$1"
    local expected="$2"
    local cmds="$3"
    
    echo ""
    echo "[$test_name]"
    
    rm -f disk.img
    local result=$(echo -e "$cmds" | ./filesys 1 2>&1)
    
    if echo "$result" | grep -q "$expected"; then
        echo "✓ PASS"
        TEST_PASS=$((TEST_PASS + 1))
    else
        echo "✗ FAIL"
        echo "  期望: $expected"
        echo "  实际:"
        echo "$result" | grep -E "(登录|创建|移入|映射|权限|日志|清空)" | head -3
        TEST_FAIL=$((TEST_FAIL + 1))
    fi
}

run_test "1. 用户登录" "登录成功" "login root\nroot\nexit"
run_test "2. 创建文件" "创建成功" "login root\nroot\ncreate test.txt\nexit"
run_test "3. 创建目录" "创建成功" "login root\nroot\nmkdir testdir\nexit"
run_test "4. 列出目录" "test.txt" "login root\nroot\ncreate test.txt\ndir\nexit"
run_test "5. 写入文件" "写入" "login root\nroot\ncreate f.txt\nopen f.txt w\nwrite 0 hello\nclose 0\nexit"
run_test "6. 读取文件" "hello" "login root\nroot\ncreate f.txt\nopen f.txt w\nwrite 0 hello\nclose 0\nopen f.txt r\nread 0 10\nexit"
run_test "7. 移入回收站" "移入回收站" "login root\nroot\ncreate delme.txt\ntrash-move delme.txt\nexit"
run_test "8. 查看回收站" "delme" "login root\nroot\ncreate delme.txt\ntrash-move delme.txt\ntrash\nexit"
run_test "9. 清空回收站" "清空" "login root\nroot\ncreate temp.txt\ntrash-move temp.txt\ntrash-empty\nexit"
run_test "10. mmap映射" "映射成功" "login root\nroot\ncreate mmap.txt\nopen mmap.txt w\nwrite 0 testdata\nclose 0\nopen mmap.txt r\nmmap 0 100\nexit"
run_test "11. 日志功能" "CREATE" "login root\nroot\ncreate logtest.txt\njournal\nexit"
run_test "12. 修改权限" "权限已修改" "login root\nroot\ncreate perm.txt\nchmod perm.txt 755\nexit"

echo ""
echo "=============================================="
echo "  测试结果: PASS=$TEST_PASS, FAIL=$TEST_FAIL"
echo "=============================================="

if [ $TEST_FAIL -eq 0 ]; then
    echo "✓ 所有测试通过!"
    exit 0
else
    echo "✗ 有 $TEST_FAIL 个测试失败"
    exit 1
fi