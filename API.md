# Linux 二级文件系统 API 文档

## 一、文件系统操作

### 1.1 初始化

```c
int fs_init(const char *path);
```

初始化文件系统，加载或创建磁盘镜像。

### 1.2 关闭

```c
void fs_close(void);
```

关闭文件系统，同步所有数据。

## 二、块操作

### 2.1 分配块

```c
int block_alloc(void);
```

返回：块号（成功），-1（失败）

### 2.2 释放块

```c
void block_free(uint32_t blk);
```

### 2.3 读取块

```c
void read_block(uint32_t blk, char *buf);
```

### 2.4 写入块

```c
void write_block(uint32_t blk, const char *buf);
```

### 2.5 检查块使用状态

```c
int block_is_used(uint32_t blk);
```

返回：1（已使用），0（空闲）

## 三、Inode 操作

### 3.1 分配 inode

```c
int inode_alloc(void);
```

返回：inode 号（成功），-1（失败）

### 3.2 释放 inode

```c
void inode_free(uint32_t ino);
```

### 3.3 读取 inode

```c
void read_inode(uint32_t ino, struct inode *ip);
```

### 3.4 写入 inode

```c
void write_inode(uint32_t ino, const struct inode *ip);
```

### 3.5 获取块号

```c
uint32_t get_block_by_offset(uint32_t ino, uint32_t offset, int alloc);
```

## 四、目录操作

### 4.1 创建目录

```c
int dir_create(const char *name, uint32_t parent_ino);
```

返回：新目录 inode 号（成功），-1（失败）

### 4.2 删除目录

```c
int dir_remove(const char *name, uint32_t parent_ino);
```

返回：0（成功），-1（失败）

### 4.3 查找目录项

```c
uint32_t dir_lookup(uint32_t dir_ino, const char *name);
```

返回：inode 号（成功），0（失败）

### 4.4 添加目录项

```c
void dir_add_entry(uint32_t dir_ino, uint32_t ino, const char *name, uint8_t ftype);
```

### 4.5 删除目录项

```c
void dir_remove_entry(uint32_t dir_ino, const char *name);
```

### 4.6 列出目录

```c
int dir_list(uint32_t dir_ino);
```

### 4.7 创建硬链接

```c
int dir_link(const char *target, const char *linkname);
```

返回：0（成功），-1（失败）

### 4.8 创建软链接

```c
int dir_symlink(const char *target, const char *linkname);
```

返回：0（成功），-1（失败）

### 4.9 读取软链接

```c
int dir_readlink(uint32_t ino, char *buf, size_t size);
```

返回：链接目标长度（成功），-1（失败）

## 五、文件操作

### 5.1 创建文件

```c
int file_create(const char *name, mode_t mode);
```

返回：0（成功），-1（失败）

### 5.2 删除文件

```c
int file_delete(const char *name);
```

返回：0（成功），-1（失败）

### 5.3 打开文件

```c
int file_open(const char *name, uint16_t mode);
```

返回：文件描述符（成功），-1（失败）

### 5.4 关闭文件

```c
int file_close(int fd);
```

返回：0（成功），-1（失败）

### 5.5 读取文件

```c
int file_read(int fd, char *buf, int size);
```

返回：读取字节数（成功），0（EOF），-1（失败）

### 5.6 写入文件

```c
int file_write(int fd, const char *buf, int size);
```

返回：写入字节数（成功），-1（失败）

## 六、用户操作

### 6.1 用户登录

```c
int user_login(const char *name, const char *passwd);
```

返回：0（成功），-1（失败）

### 6.2 用户注册

```c
int user_register(const char *name, const char *passwd);
```

返回：0（成功），-1（失败）

## 七、回收站操作

### 7.1 移入回收站

```c
int trash_move(const char *path);
```

返回：0（成功），-1（失败）

### 7.2 恢复文件

```c
int trash_restore(const char *name);
```

返回：0（成功），-1（失败）

### 7.3 清空回收站

```c
void trash_empty(void);
```

### 7.4 列出回收站

```c
void trash_list(void);
```

## 八、日志操作

### 8.1 添加日志

```c
void journal_add(int type, uint32_t inode, uint32_t block, const char *data);
```

### 8.2 列出日志

```c
void journal_list(void);
```

### 8.3 清空日志

```c
void journal_clear(void);
```

### 8.4 切换日志

```c
void journal_toggle(int enable);
```

## 九、缓存操作

### 9.1 缓存读取

```c
void cache_read_block(uint32_t block_no, char *buf);
```

### 9.2 缓存写入

```c
void cache_write_block(uint32_t block_no, const char *buf);
```

### 9.3 刷新缓存

```c
void cache_flush(void);
```

### 9.4 清空缓存

```c
void cache_clear(void);
```

## 十、错误码

| 错误码 | 含义 |
|--------|------|
| -1 | 操作失败 |
| 0 | 操作成功 |
| 1 | 资源已存在 |
| 2 | 资源不存在 |
| 3 | 权限不足 |
| 4 | 系统错误 |

## 十一、数据类型

### 11.1 文件类型

```c
#define FT_UNKNOWN   0    /* 未知 */
#define FT_REG_FILE  1    /* 普通文件 */
#define FT_DIRECTORY 2    /* 目录 */
#define FT_SYMLINK   4    /* 软链接 */
```

### 11.2 打开模式

```c
#define OPEN_READ    0x01
#define OPEN_WRITE   0x02
#define OPEN_RDWR    0x03
```