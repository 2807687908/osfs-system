#!/bin/bash

echo "=================================================="
echo "  Linux二级文件系统 - 环境配置脚本"
echo "  支持: VMware Ubuntu / WSL"
echo "=================================================="

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第一步: 检测运行环境】"
echo "──────────────────────────────────────────────────"

if grep -qi microsoft /proc/version; then
    ENV_TYPE="WSL"
    echo -e "${YELLOW}检测到 WSL 环境${NC}"
else
    ENV_TYPE="VMware"
    echo -e "${GREEN}检测到 VMware/Ubuntu 环境${NC}"
fi

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第二步: 安装依赖】"
echo "──────────────────────────────────────────────────"

echo ""
echo "安装编译依赖..."
sudo apt-get update > /dev/null 2>&1
sudo apt-get install -y gcc make libncurses5-dev libncursesw5-dev > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "     ${GREEN}✓ 依赖安装成功${NC}"
else
    echo -e "     ${RED}✗ 依赖安装失败${NC}"
    exit 1
fi

echo ""
echo "检查gcc版本..."
gcc_version=$(gcc --version | head -1 | awk '{print $3}')
echo -e "     ${GREEN}✓ GCC版本: $gcc_version${NC}"

echo ""
echo "检查ncurses版本..."
ncurses_version=$(dpkg -l libncurses5 2>/dev/null | grep -oP '(\d+\.\d+)' | head -1)
echo -e "     ${GREEN}✓ ncurses版本: $ncurses_version${NC}"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第三步: 编译项目】"
echo "──────────────────────────────────────────────────"

cd /home/ubuntu1/osfs
echo ""
echo "清理旧编译文件..."
make clean > /dev/null 2>&1
echo -e "     ${GREEN}✓ 清理完成${NC}"

echo ""
echo "编译项目..."
make 2>&1
if [ $? -eq 0 ]; then
    echo -e "     ${GREEN}✓ 编译成功${NC}"
    echo -e "     ${GREEN}✓ 可执行文件: /home/ubuntu1/osfs/filesys${NC}"
else
    echo -e "     ${RED}✗ 编译失败${NC}"
    exit 1
fi

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第四步: 环境配置】"
echo "──────────────────────────────────────────────────"

if [ "$ENV_TYPE" = "WSL" ]; then
    echo ""
    echo "WSL特殊配置:"
    echo "  - 设置DISPLAY环境变量..."
    if [ -z "$DISPLAY" ]; then
        export DISPLAY=:0
        echo "    export DISPLAY=:0" >> ~/.bashrc
        echo -e "     ${GREEN}✓ DISPLAY已配置${NC}"
    else
        echo -e "     ${GREEN}✓ DISPLAY已设置: $DISPLAY${NC}"
    fi
    
    echo ""
    echo "  - 检查X Server..."
    if ! command -v xclock &> /dev/null; then
        echo -e "     ${YELLOW}⚠ 未检测到X Server${NC}"
        echo "     请安装VcXsrv或其他X Server:"
        echo "     1. 下载: https://sourceforge.net/projects/vcxsrv/"
        echo "     2. 运行XLaunch，选择"Disable access control""
    else
        echo -e "     ${GREEN}✓ X Server已安装${NC}"
    fi
else
    echo ""
    echo "VMware Ubuntu配置:"
    echo "  - 检查图形环境..."
    if [ -n "$DISPLAY" ]; then
        echo -e "     ${GREEN}✓ 图形环境正常: $DISPLAY${NC}"
    else
        echo -e "     ${YELLOW}⚠ 图形环境未配置${NC}"
        echo "     请安装桌面环境:"
        echo "     sudo apt-get install ubuntu-desktop"
    fi
fi

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第五步: 快速测试】"
echo "──────────────────────────────────────────────────"

echo ""
echo "运行命令行模式测试..."
rm -f disk.img
result=$(echo -e "login root\nroot\ncreate test.txt\ndir\nexit" | ./filesys 1 2>&1)
if echo "$result" | grep -q "test.txt"; then
    echo -e "     ${GREEN}✓ 命令行模式测试通过${NC}"
else
    echo -e "     ${RED}✗ 命令行模式测试失败${NC}"
    exit 1
fi

echo ""
echo "──────────────────────────────────────────────────"
echo " 【配置完成】"
echo "──────────────────────────────────────────────────"
echo ""
echo "使用方法:"
echo ""
echo "命令行模式:"
echo "  cd /home/ubuntu1/osfs"
echo "  ./filesys 1"
echo ""
echo "图形化模式:"
echo "  cd /home/ubuntu1/osfs"
echo "  ./filesys 2"
echo ""
echo "运行自动化测试:"
echo "  cd /home/ubuntu1/osfs"
echo "  ./test_comprehensive.sh"
echo ""
echo "GUI测试指南:"
echo "  cd /home/ubuntu1/osfs"
echo "  ./test_gui.sh"
echo ""
echo "=================================================="
