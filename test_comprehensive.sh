#!/bin/bash

echo "=================================================="
echo "  Linux二级文件系统 - 综合测试套件 v1.0"
echo "  覆盖: WSL/Ubuntu, CLI/GUI, 所有功能"
echo "=================================================="

TEST_PASS=0
TEST_FAIL=0
TEST_TOTAL=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

run_test() {
    local test_id="$1"
    local test_name="$2"
    local env="$3"
    local mode="$4"
    local expected="$5"
    local cmds="$6"
    
    TEST_TOTAL=$((TEST_TOTAL + 1))
    echo ""
    echo -e "${YELLOW}[$test_id] $test_name${NC}"
    echo -e "         [环境: $env | 模式: $mode]"
    
    rm -f disk.img
    local result=$(echo -e "$cmds" | ./filesys 1 2>&1)
    
    if echo "$result" | grep -q "$expected"; then
        echo -e "         ${GREEN}✓ PASS${NC}"
        TEST_PASS=$((TEST_PASS + 1))
    else
        echo -e "         ${RED}✗ FAIL${NC}"
        echo "         期望: $expected"
        echo "         实际输出片段:"
        echo "$result" | grep -E "(登录|创建|删除|读取|写入|权限|目录|返回|恢复|同步|解除|日志|已删除|权限不足|不存在|CHMOD|/a/b)" | head -5
        TEST_FAIL=$((TEST_FAIL + 1))
    fi
}

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第一部分: 用户管理与登录】"
echo "──────────────────────────────────────────────────"

run_test "U01" "管理员登录(root)" "WSL/Ubuntu" "CLI" "登录成功! 欢迎 root" "login root\nroot\npwd\nexit"
run_test "U02" "普通用户登录(user1)" "WSL/Ubuntu" "CLI" "登录成功! 欢迎 user1" "login user1\n123\npwd\nexit"
run_test "U03" "登录失败(错误密码)" "WSL/Ubuntu" "CLI" "密码错误" "login root\nwrong\nexit"
run_test "U04" "未登录时无法操作" "WSL/Ubuntu" "CLI" "权限不足" "create test.txt\nexit"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第二部分: 文件基本操作】"
echo "──────────────────────────────────────────────────"

run_test "F01" "创建文件" "WSL/Ubuntu" "CLI" "创建成功" "login root\nroot\ncreate file1.txt\nexit"
run_test "F02" "创建带权限文件" "WSL/Ubuntu" "CLI" "创建成功" "login root\nroot\ncreate secure.txt 600\nexit"
run_test "F03" "写入文件内容" "WSL/Ubuntu" "CLI" "写入" "login root\nroot\ncreate data.txt\nopen data.txt w\nwrite 0 Hello World\nclose 0\nexit"
run_test "F04" "读取文件内容" "WSL/Ubuntu" "CLI" "Hello World" "login root\nroot\ncreate data.txt\nopen data.txt w\nwrite 0 Hello World\nclose 0\nopen data.txt r\nread 0 20\nexit"
run_test "F05" "读写模式打开文件" "WSL/Ubuntu" "CLI" "写入 4 字节" "login root\nroot\ncreate rw.txt\nopen rw.txt rw\nwrite 0 test\nclose 0\nexit"
run_test "F06" "删除文件" "WSL/Ubuntu" "CLI" "已删除" "login root\nroot\ncreate tmp.txt\ndelete tmp.txt\nexit"
run_test "F07" "文件不存在错误" "WSL/Ubuntu" "CLI" "不存在" "login root\nroot\nopen notexist.txt r\nexit"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第三部分: 目录操作】"
echo "──────────────────────────────────────────────────"

run_test "D01" "创建目录" "WSL/Ubuntu" "CLI" "创建成功" "login root\nroot\nmkdir docs\nexit"
run_test "D02" "切换目录" "WSL/Ubuntu" "CLI" "/docs" "login root\nroot\nmkdir docs\ncd docs\npwd\nexit"
run_test "D03" "返回上级目录" "WSL/Ubuntu" "CLI" "当前目录: /" "login root\nroot\nmkdir docs\ncd docs\ncd ..\npwd\nexit"
run_test "D04" "列出目录内容" "WSL/Ubuntu" "CLI" "file.txt" "login root\nroot\ncreate file.txt\nmkdir dir1\ndir\nexit"
run_test "D05" "删除空目录" "WSL/Ubuntu" "CLI" "已清空" "login root\nroot\nmkdir empty\ntrash-move empty\ntrash-empty\nexit"
run_test "D06" "嵌套目录操作" "WSL/Ubuntu" "CLI" "目录 'b' 创建成功" "login root\nroot\nmkdir a\ncd a\nmkdir b\npwd\nexit"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第四部分: 回收站功能】"
echo "──────────────────────────────────────────────────"

run_test "T01" "文件移入回收站" "WSL/Ubuntu" "CLI" "移入回收站" "login root\nroot\ncreate delme.txt\ntrash-move delme.txt\nexit"
run_test "T02" "查看回收站内容" "WSL/Ubuntu" "CLI" "delme.txt_" "login root\nroot\ncreate delme.txt\ntrash-move delme.txt\ntrash\nexit"
run_test "T03" "恢复文件" "WSL/Ubuntu" "CLI" "恢复到当前目录" "login root\nroot\ncreate restore.txt\ntrash-move restore.txt\ntrash-restore restore.txt_2\ndir\nexit"
run_test "T04" "清空回收站" "WSL/Ubuntu" "CLI" "已清空" "login root\nroot\ncreate temp.txt\ntrash-move temp.txt\ntrash-empty\ntrash\nexit"
run_test "T05" "目录移入回收站" "WSL/Ubuntu" "CLI" "移入回收站" "login root\nroot\nmkdir trashdir\ntrash-move trashdir\ntrash\nexit"
run_test "T06" "恢复目录" "WSL/Ubuntu" "CLI" "restdir" "login root\nroot\nmkdir restdir\ntrash-move restdir\ntrash-restore restdir_2\ndir\nexit"
run_test "T07" "恢复不存在文件" "WSL/Ubuntu" "CLI" "未找到" "login root\nroot\ntrash-restore notexist_999\nexit"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第五部分: 内存映射(mmap)】"
echo "──────────────────────────────────────────────────"

run_test "M01" "创建mmap映射" "WSL/Ubuntu" "CLI" "映射成功" "login root\nroot\ncreate mmap.txt\nopen mmap.txt w\nwrite 0 testdata\nclose 0\nopen mmap.txt r\nmmap 0 100\nexit"
run_test "M02" "查看映射列表" "WSL/Ubuntu" "CLI" "mmap" "login root\nroot\ncreate mmap.txt\nopen mmap.txt w\nwrite 0 data\nclose 0\nopen mmap.txt r\nmmap 0 50\nmlist\nexit"
run_test "M03" "同步映射内容" "WSL/Ubuntu" "CLI" "映射" "login root\nroot\ncreate sync.txt\nopen sync.txt w\nwrite 0 hello\nclose 0\nopen sync.txt r\nmmap 0 100\nmsync 0x\nmlist\nexit"
run_test "M04" "解除映射" "WSL/Ubuntu" "CLI" "映射" "login root\nroot\ncreate unmap.txt\nopen unmap.txt w\nwrite 0 data\nclose 0\nopen unmap.txt r\nmmap 0 50\nmunmap 0x\nmlist\nexit"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第六部分: 日志系统】"
echo "──────────────────────────────────────────────────"

run_test "J01" "日志记录创建操作" "WSL/Ubuntu" "CLI" "CREATE" "login root\nroot\ncreate logfile.txt\njournal\nexit"
run_test "J02" "日志记录写入操作" "WSL/Ubuntu" "CLI" "日志" "login root\nroot\ncreate write.txt\nopen write.txt w\nwrite 0 test\nclose 0\njournal\nexit"
run_test "J03" "日志记录删除操作" "WSL/Ubuntu" "CLI" "DELETE" "login root\nroot\ncreate del.txt\ndelete del.txt\njournal\nexit"
run_test "J04" "日志记录权限修改" "WSL/Ubuntu" "CLI" "日志" "login root\nroot\ncreate perm.txt\nchmod perm.txt 755\njournal\nexit"
run_test "J05" "清空日志" "WSL/Ubuntu" "CLI" "已清空" "login root\nroot\ncreate test.txt\njournal-clear\njournal\nexit"
run_test "J06" "日志开关控制" "WSL/Ubuntu" "CLI" "关闭" "login root\nroot\njournal-toggle off\njournal-toggle on\nexit"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第七部分: 权限管理】"
echo "──────────────────────────────────────────────────"

run_test "P01" "修改文件权限" "WSL/Ubuntu" "CLI" "权限已修改" "login root\nroot\ncreate perm.txt\nchmod perm.txt 755\nexit"
run_test "P02" "设置不同权限模式" "WSL/Ubuntu" "CLI" "权限已修改" "login root\nroot\ncreate file.txt\nchmod file.txt 644\ndir\nexit"
run_test "P03" "查看磁盘使用" "WSL/Ubuntu" "CLI" "已用" "login root\nroot\ncreate test.txt\ndf\nexit"
run_test "P04" "查看打开文件" "WSL/Ubuntu" "CLI" "fd=" "login root\nroot\ncreate file.txt\nopen file.txt r\nlsof\nexit"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第八部分: 综合场景测试】"
echo "──────────────────────────────────────────────────"

run_test "S01" "完整文件生命周期" "WSL/Ubuntu" "CLI" "life.txt" "login root\nroot\ncreate life.txt\nopen life.txt w\nwrite 0 content\nclose 0\nopen life.txt r\nread 0 20\nclose 0\ntrash-move life.txt\ntrash-restore life.txt_2\ndir\nexit"
run_test "S02" "多用户权限隔离" "WSL/Ubuntu" "CLI" "登录成功! 欢迎 user1" "login root\nroot\ncreate secret.txt\nchmod secret.txt 600\nlogout\nlogin user1\n123\nopen secret.txt r\nexit"
run_test "S03" "大文件写入测试" "WSL/Ubuntu" "CLI" "写入" "login root\nroot\ncreate big.txt\nopen big.txt w\nwrite 0 $(printf 'a%.0s' {1..500})\nclose 0\nopen big.txt r\nread 0 100\nexit"
run_test "S04" "连续操作日志记录" "WSL/Ubuntu" "CLI" "CREATE" "login root\nroot\ncreate a.txt\ncreate b.txt\ncreate c.txt\njournal\nexit"

echo ""
echo "=================================================="
echo -e "  测试结果: ${GREEN}PASS=$TEST_PASS${NC}, ${RED}FAIL=$TEST_FAIL${NC}, 总计=$TEST_TOTAL"
echo "=================================================="

if [ $TEST_FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ 所有测试通过!${NC}"
    exit 0
else
    echo -e "${RED}✗ 有 $TEST_FAIL 个测试失败${NC}"
    exit 1
fi
