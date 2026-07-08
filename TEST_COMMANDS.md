# Linux二级文件系统 - v3版本测试指令清单

## 目录
1. [版本信息](#版本信息)
2. [命令行模式测试指令](#命令行模式测试指令)
3. [GUI模式测试指令](#gui模式测试指令)
4. [功能验证清单](#功能验证清单)
5. [自动化测试脚本](#自动化测试脚本)

---

## 版本信息
- **版本号**: v3
- **更新内容**:
  - 新增单元测试框架（CUnit）及核心模块测试用例
  - 完善错误处理：增加边界检查、错误返回机制
  - 性能优化：块缓存机制（FIFO策略）、延迟写入
  - 扩展软硬链接功能：支持硬链接和软链接创建与验证
  - 完善设计文档（DESIGN.md）和API文档（API.md）

---

## 命令行模式测试指令

### 启动方式
```bash
cd /home/ubuntu1/osfs
./filesys 1
```

### 1. 用户登录测试
```bash
login root
root
login user1
123
logout
```

### 2. 文件操作测试
```bash
# 创建文件
create test.txt
create secure.txt 600

# 打开文件
open test.txt r
open test.txt w
open test.txt rw

# 写入文件
write 0 Hello World
write 0 这是测试内容

# 读取文件
read 0 20
read 0 100

# 关闭文件
close 0

# 删除文件
delete test.txt
```

### 3. 目录操作测试
```bash
# 创建目录
mkdir docs
mkdir project/src

# 切换目录
cd docs
cd ..
cd project/src

# 列出目录
dir
dir /docs

# 删除目录
rmdir empty_dir
```

### 4. 回收站功能测试
```bash
# 创建并移入回收站
create delme.txt
trash-move delme.txt

# 查看回收站
trash

# 恢复文件（使用trash显示的文件名）
trash-restore delme.txt_2

# 清空回收站
trash-empty
```

### 5. 内存映射测试
```bash
# 创建文件并写入内容
create mmap.txt
open mmap.txt w
write 0 test data
close 0

# 映射到内存
open mmap.txt r
mmap 0 100

# 查看映射列表
mlist

# 同步映射内容
msync 0x7fxxxxxxxx

# 解除映射
munmap 0x7fxxxxxxxx
```

### 6. 日志系统测试
```bash
# 查看日志
journal

# 清空日志
journal-clear

# 开关日志
journal-toggle off
journal-toggle on
```

### 7. 权限管理测试
```bash
# 修改权限
chmod test.txt 755
chmod secret.txt 600

# 查看磁盘使用
df

# 查看打开文件
lsof
```

### 8. 软硬链接功能测试（v3新增）

#### 8.1 硬链接测试
```bash
# 创建源文件
create source.txt
open source.txt w
write 0 Hello Hard Link
close 0

# 创建硬链接
ln source.txt link_hard

# 查看目录（硬链接与源文件共享同一inode）
dir
# 预期结果：
# source.txt 和 link_hard 的 inode 号相同
# 类型均为 <FILE>

# 通过硬链接读取内容
open link_hard r
read 0 20
close 0
# 预期结果：读取到 "Hello Hard Link"

# 通过硬链接写入内容
open link_hard w
write 0 Modified via hard link
close 0

# 通过源文件验证修改
open source.txt r
read 0 30
close 0
# 预期结果：读取到 "Modified via hard link"

# 删除硬链接（源文件仍存在）
delete link_hard
dir
# 预期结果：source.txt 仍存在

# 删除源文件
delete source.txt
dir
# 预期结果：source.txt 被删除
```

#### 8.2 软链接测试
```bash
# 创建源文件
create target.txt
open target.txt w
write 0 Hello Soft Link
close 0

# 创建软链接
ln -s target.txt link_soft

# 查看目录（软链接有独立inode）
dir
# 预期结果：
# target.txt 的 inode 号与 link_soft 不同
# link_soft 的类型为 <LINK>
# link_soft 的大小为目标路径长度

# 通过软链接读取内容
open link_soft r
read 0 20
close 0
# 预期结果：读取到 "Hello Soft Link"

# 删除源文件（软链接变成"断链"）
delete target.txt
dir
# 预期结果：link_soft 仍存在，但指向已删除的文件

# 删除软链接
delete link_soft
dir
# 预期结果：link_soft 被删除
```

#### 8.3 链接错误处理测试
```bash
# 创建硬链接到不存在的文件
ln nonexistent.txt link1
# 预期结果：显示 "目标文件 'nonexistent.txt' 不存在"

# 创建软链接到不存在的文件
ln -s nonexistent.txt link2
# 预期结果：软链接创建成功（允许指向不存在的目标）

# 创建硬链接到目录（不支持）
mkdir test_dir
ln test_dir dir_link
# 预期结果：显示 "不支持对目录创建硬链接"

# 创建同名链接
create file.txt
ln file.txt link_same
ln file.txt link_same
# 预期结果：第二次创建显示 "链接文件 'link_same' 已存在"
```

### 9. 错误处理测试（v3完善）

#### 9.1 边界检查测试
```bash
# inode号超出范围
# 预期结果：内部会进行范围检查，不会崩溃

# 块号超出范围
# 预期结果：内部会进行范围检查，不会崩溃

# 空文件名
create ""
# 预期结果：显示错误信息

# 超长文件名（超过255字符）
create $(python3 -c "print('a'*260)")
# 预期结果：显示错误信息或截断
```

#### 9.2 权限检查测试
```bash
# 未登录时执行操作
logout
create test.txt
# 预期结果：显示 "权限不足: 无法创建文件"

mkdir docs
# 预期结果：显示 "权限不足: 无法在目录中创建子目录"

# 普通用户操作
login user1 123
create userfile.txt
# 预期结果：创建成功

# 尝试删除root文件
delete rootfile.txt
# 预期结果：显示权限错误（如果文件属于root）
```

### 10. 性能优化测试（v3新增）

#### 10.1 块缓存测试
```bash
# 创建大文件（触发多次块分配）
create bigfile.txt
open bigfile.txt w
write 0 $(python3 -c "print('x'*4096)")
write 0 $(python3 -c "print('y'*4096)")
write 0 $(python3 -c "print('z'*4096)")
close 0

# 重复读取同一文件（测试缓存命中）
open bigfile.txt r
read 0 4096
read 0 4096
read 0 4096
close 0
# 预期结果：后续读取速度更快（缓存生效）
```

#### 10.2 延迟写入测试
```bash
# 多次写入同一文件
create delayed.txt
open delayed.txt w
write 0 First write
write 0 Second write
write 0 Third write
# 预期结果：多次写入合并为更少的磁盘I/O操作
close 0
```

### 11. 综合测试流程（v3完整版）
```bash
login root root
create file.txt
open file.txt w
write 0 Hello World
close 0
open file.txt r
read 0 20
close 0
mkdir docs
cd docs
pwd
cd ..

# v3新增：测试软硬链接
ln file.txt link_hard
ln -s file.txt link_soft
dir

# v3新增：测试链接读写
open link_hard r
read 0 20
close 0
open link_soft r
read 0 20
close 0

trash-move file.txt
trash
trash-restore file.txt_2
journal
delete link_hard
delete link_soft
logout
exit
```

---

## GUI模式测试指令

### 启动方式
```bash
# VMware Ubuntu
cd /home/ubuntu1/osfs
./filesys 2

# WSL（需配置X Server）
export DISPLAY=:0
cd /home/ubuntu1/osfs
./filesys 2
```

### GUI界面布局
```
┌─────────────────────────────────────────────┐
│ /home/user/documents                        │
│                                             │
│ Name                     Inode    Size     │
│ -------------------------------------------│
│ .                        1        32       │
│ ..                       1        32       │
│ test.txt                 2        0        │
│                                             │
└─────────────────────────────────────────────┘
┌─────────────────────────────────────────────┐
│ Open FDs: fd=0(inode=2,mode=r)             │
│ Use: open <file> r/w/rw | read/write <fd>   │
└─────────────────────────────────────────────┘
┌─────────────────────────────────────────────┐
│ User: root | Used: 128 | Free: 32640       │
│ Selected: test.txt (<FILE>, inode=2, 0 bytes)│
└─────────────────────────────────────────────┘
┌─────────────────────────────────────────────┐
│ > open test.txt r                          │
└─────────────────────────────────────────────┘
```

### GUI测试步骤

#### 步骤1：登录
```bash
# 在命令行输入
login root
# 输入密码
root
# 预期：登录成功，显示文件列表
```

#### 步骤2：创建文件
```bash
create test.txt
# 预期：文件列表显示 test.txt
```

#### 步骤3：创建目录
```bash
mkdir docs
# 预期：文件列表显示 docs <DIR>
```

#### 步骤4：打开文件
```bash
open test.txt r
# 预期：FD面板显示 fd=0(inode=?,mode=r)
```

#### 步骤5：写入文件
```bash
write 0 Hello World
# 预期：显示"写入 11 字节"
```

#### 步骤6：读取文件
```bash
read 0 20
# 预期：显示"读取 11 字节"和内容"Hello World"
```

#### 步骤7：关闭文件
```bash
close 0
# 预期：FD面板不再显示该fd
```

#### 步骤8：目录切换
```bash
cd docs
# 预期：路径显示变为 /docs
```

#### 步骤9：回收站操作
```bash
trash-move test.txt
# 预期：文件列表移除 test.txt
```

#### 步骤10：查看回收站
```bash
trash
# 预期：命令行显示回收站内容列表
```

#### 步骤11：恢复文件
```bash
# 根据trash显示的文件名恢复
trash-restore test.txt_2
# 预期：文件列表重新显示 test.txt
```

#### 步骤12：查看日志
```bash
journal
# 预期：命令行显示操作日志列表
```

#### 步骤13：修改权限
```bash
chmod test.txt 755
# 预期：文件权限变为 rwxr-xr-x
```

#### 步骤14：mmap操作
```bash
open test.txt r
mmap 0 100
# 预期：显示"映射成功: 地址"
```

#### 步骤15：创建硬链接（v3新增）
```bash
ln test.txt link_hard
# 预期：文件列表显示 link_hard，与 test.txt 相同 inode
```

#### 步骤16：创建软链接（v3新增）
```bash
ln -s test.txt link_soft
# 预期：文件列表显示 link_soft <LINK>，有独立 inode
```

#### 步骤17：通过链接访问文件（v3新增）
```bash
open link_hard r
read 0 20
# 预期：读取到 test.txt 的内容

open link_soft r
read 0 20
# 预期：读取到 test.txt 的内容
```

#### 步骤18：退出
```bash
exit
# 预期：程序正常退出，返回终端
```

---

## 功能验证清单

### 用户管理
- [x] 管理员登录 (root/root)
- [x] 普通用户登录 (user1/123)
- [x] 登录失败处理
- [x] 未登录权限检查

### 文件操作
- [x] 创建文件
- [x] 创建带权限文件
- [x] 打开文件（读/写/读写模式）
- [x] 写入文件内容
- [x] 读取文件内容
- [x] 关闭文件
- [x] 删除文件
- [x] 文件不存在错误处理

### 目录操作
- [x] 创建目录
- [x] 切换目录
- [x] 返回上级目录
- [x] 列出目录内容
- [x] 删除空目录
- [x] 嵌套目录操作

### 回收站功能
- [x] 文件移入回收站
- [x] 目录移入回收站
- [x] 查看回收站内容
- [x] 恢复文件（`文件名_inode号`格式）
- [x] 恢复目录
- [x] 清空回收站
- [x] 恢复不存在文件错误处理

### 内存映射
- [x] 创建mmap映射
- [x] 查看映射列表
- [x] 同步映射内容
- [x] 解除映射

### 日志系统
- [x] 记录CREATE操作
- [x] 记录DELETE操作
- [x] 记录WRITE操作
- [x] 记录CHMOD操作
- [x] 查看日志列表
- [x] 清空日志
- [x] 日志开关控制

### 权限管理
- [x] 修改文件权限
- [x] 查看磁盘使用情况
- [x] 查看打开文件描述符

### 软硬链接（v3新增）
- [x] 创建硬链接（ln）
- [x] 创建软链接（ln -s）
- [x] 硬链接共享inode验证
- [x] 软链接独立inode验证
- [x] 通过硬链接读写文件
- [x] 通过软链接读写文件
- [x] 删除硬链接不影响源文件
- [x] 删除源文件后软链接变断链
- [x] 创建链接到不存在文件的错误处理
- [x] 创建同名链接的错误处理

### 错误处理（v3完善）
- [x] inode号范围检查
- [x] 块号范围检查
- [x] 空文件名检查
- [x] 超长文件名处理
- [x] 未登录权限检查
- [x] 权限不足错误提示

### 性能优化（v3新增）
- [x] 块缓存机制（FIFO策略）
- [x] 延迟写入优化
- [x] 缓存命中率统计

---

## 自动化测试脚本

### 运行所有测试
```bash
cd /home/ubuntu1/osfs

# 基础测试（12个用例）
./test_all.sh

# 综合测试（42个用例）
./test_comprehensive.sh
```

### 测试脚本说明

| 脚本 | 用例数 | 覆盖范围 |
|------|--------|---------|
| `test_all.sh` | 12 | 基础功能测试 |
| `test_comprehensive.sh` | 42 | 完整功能覆盖测试 |
| `test_gui.sh` | 15 | GUI模式手动测试指南 |
| `setup_env.sh` | - | 环境配置脚本 |

### 预期测试结果
```
测试结果: PASS=42, FAIL=0, 总计=42
✓ 所有测试通过!
```

---

## 版本标签

```bash
# 查看所有版本
git tag

# v0: 初始版本
# v1: 添加GUI、日志、回收站、mmap功能
# v2: 修复日志功能、GUI操作、fd显示、回收站文件名格式
# v3: 单元测试、错误处理完善、性能优化、软硬链接功能、设计文档和API文档
```
