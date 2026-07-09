#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "=================================================="
echo "  Git仓库永久配置脚本"
echo "  仓库: https://github.com/2807687908/osfs-system"
echo "=================================================="

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第一步: 配置远程仓库】"
echo "──────────────────────────────────────────────────"

cd "$(dirname "$0")"

echo ""
echo "设置远程仓库地址..."
git remote set-url origin https://github.com/2807687908/osfs-system.git
echo -e "     ${GREEN}✓ 远程仓库: https://github.com/2807687908/osfs-system.git${NC}"

echo ""
echo "设置上游分支..."
git branch --set-upstream-to=origin/master master
echo -e "     ${GREEN}✓ 上游分支已设置${NC}"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第二步: 配置DNS解析】"
echo "──────────────────────────────────────────────────"

GITHUB_IP="20.205.243.166"

echo ""
echo "检查hosts文件中是否已有GitHub IP..."
if grep -q "^$GITHUB_IP.*github\.com" /etc/hosts 2>/dev/null; then
    echo -e "     ${GREEN}✓ GitHub IP已配置${NC}"
else
    echo -e "     ${YELLOW}⚠ 未找到GitHub IP，需要添加${NC}"
    echo ""
    echo "正在添加GitHub IP到hosts文件..."
    echo "$GITHUB_IP github.com" | sudo tee -a /etc/hosts
    echo "$GITHUB_IP ssh.github.com" | sudo tee -a /etc/hosts
    if [ $? -eq 0 ]; then
        echo -e "     ${GREEN}✓ GitHub IP添加成功${NC}"
    else
        echo -e "     ${RED}✗ 添加失败，请手动执行:${NC}"
        echo ""
        echo "  sudo bash -c 'echo \"$GITHUB_IP github.com\" >> /etc/hosts'"
        echo "  sudo bash -c 'echo \"$GITHUB_IP ssh.github.com\" >> /etc/hosts'"
    fi
fi

echo ""
echo "验证GitHub解析..."
ping -c 1 -W 2 github.com > /dev/null 2>&1
if [ $? -eq 0 ]; then
    IP=$(getent hosts github.com | awk '{print $1}')
    echo -e "     ${GREEN}✓ GitHub解析成功: $IP${NC}"
else
    echo -e "     ${RED}✗ GitHub解析失败${NC}"
    echo ""
    echo -e "${YELLOW}请检查网络连接或手动配置hosts文件${NC}"
    exit 1
fi

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第三步: 配置Git用户信息】"
echo "──────────────────────────────────────────"

echo ""
echo "设置Git用户名..."
git config user.name "osfs-user"
echo -e "     ${GREEN}✓ 用户名为: osfs-user${NC}"

echo ""
echo "设置Git邮箱..."
git config user.email "osfs@localhost"
echo -e "     ${GREEN}✓ 邮箱为: osfs@localhost${NC}"

echo ""
echo "设置全局凭据缓存（1小时）..."
git config credential.helper 'cache --timeout=3600'
echo -e "     ${GREEN}✓ 凭据缓存已配置${NC}"

echo ""
echo "配置Git强制使用IPv4..."
git config --global http.version HTTP/1.1
git config --global core.preferIPv4 true
echo -e "     ${GREEN}✓ IPv4强制启用${NC}"

echo ""
echo "──────────────────────────────────────────────────"
echo " 【第四步: 验证连接】"
echo "──────────────────────────────────────────────────"

echo ""
echo "测试GitHub连接..."
curl -s -o /dev/null -w "HTTP状态码: %{http_code}\n" https://github.com
if curl -s -o /dev/null -w "%{http_code}" https://github.com | grep -q "200"; then
    echo -e "     ${GREEN}✓ GitHub连接成功${NC}"
else
    echo -e "     ${RED}✗ GitHub连接失败${NC}"
    echo ""
    echo -e "${YELLOW}可能的原因:${NC}"
    echo "  1. 网络连接问题"
    echo "  2. DNS解析问题（请检查hosts文件）"
    echo "  3. 需要使用代理"
    exit 1
fi

echo ""
echo "测试Git推送..."
echo "(这会推送所有本地提交到远程仓库)"
echo ""
git push origin master --tags
if [ $? -eq 0 ]; then
    echo -e "     ${GREEN}✓ Git推送成功${NC}"
else
    echo -e "     ${RED}✗ Git推送失败${NC}"
    echo ""
    echo -e "${YELLOW}请检查:${NC}"
    echo "  1. 是否有未提交的更改"
    echo "  2. 是否需要输入GitHub用户名密码"
    echo "  3. 是否需要使用GitHub Personal Access Token"
fi

echo ""
echo "──────────────────────────────────────────────────"
echo " 【配置完成】"
echo "──────────────────────────────────────────────────"
echo ""
echo "当前状态:"
echo ""
echo "  远程仓库: $(git remote get-url origin)"
echo "  当前分支: $(git branch --show-current)"
echo "  本地提交: $(git rev-parse --short HEAD)"
echo "  标签列表: $(git tag -l | tr '\n' ' ')"
echo ""
echo "使用方法:"
echo ""
echo "  # 查看状态"
echo "  git status"
echo ""
echo "  # 提交更改"
echo "  git add -A"
echo "  git commit -m \"描述你的更改\""
echo ""
echo "  # 推送到GitHub"
echo "  git push origin master"
echo "  git push origin --tags"
echo ""
echo "  # 拉取最新代码"
echo "  git pull origin master"
echo ""
echo "=================================================="
