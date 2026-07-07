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
    local cmd="$3"
    
    echo ""
    echo "[$test_name]"
    echo "命令: $cmd"
    
    local result=$(echo -e "$cmd" | ./filesys 2>&1 | tail -5)
    
    if echo "$result" | grep -q "$expected"; then
        echo "✓ PASS"
        TEST_PASS=$((TEST_PASS + 1))
    else
        echo "✗ FAIL"
        echo "  期望: $expected"
        echo "  实际: $result"
        TEST_FAIL=$((TEST_FAIL + 1))
    fi
}

echo ""
echo "======== 基础功能测试 ========"

run_test "1. 用户登录" "登录成功" "login root\nroot\nexit"
run_test "2. 创建文件" "创建成功" "login root\nroot\ncreate test.txt\nexit"
run_test "3. 创建目录" "创建成功" "login root\nroot\nmkdir testdir\nexit"
run_test "4. 列出目录" "test.txt" "login root\nroot\ndir\nexit"
run_test "5. 写入文件" "写入" "login root\nroot\ncreate f.txt\nopen f.txt w\nwrite 0 hello\nexit"
run_test "6. 读取文件" "hello" "login root\nroot\nopen f.txt r\nread 0 10\nexit"

echo ""
echo "======== 回收站功能测试 ========"

run_test "7. 移入回收站" "移入回收站" "login root\nroot\ncreate delme.txt\ntrash-move delme.txt\nexit"
run_test "8. 查看回收站" "delme" "login root\nroot\ntrash\nexit"
run_test "9. 恢复文件" "恢复" "login root\nroot\ntrash-restore delme.txt\nexit"
run_test "10. 清空回收站" "清空" "login root\nroot\ncreate temp.txt\ntrash-move temp.txt\ntrash-empty\nexit"

echo ""
echo "======== mmap功能测试 ========"

run_test "11. 文件映射" "映射成功" "login root\nroot\ncreate mmap.txt\nopen mmap.txt w\nwrite 0 testdata\nclose 0\nopen mmap.txt r\nmmap 0 100\nmlist\nexit"
run_test "12. 解除映射" "munmap成功" "login root\nroot\ncreate mmap2.txt\nopen mmap2.txt r\nmmap 0 50\nmunmap 0\nexit"

echo ""
echo "======== 日志功能测试 ========"

run_test "13. 查看日志" "CREATE" "login root\nroot\ncreate logtest.txt\njournal\nexit"
run_test "14. 清空日志" "已清空" "login root\nroot\njournal-clear\nexit"

echo ""
echo "======== 权限测试 ========"

run_test "15. 修改权限" "权限已修改" "login root\nroot\ncreate perm.txt\nchmod perm.txt 755\nexit"

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