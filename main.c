/*
 * Linux二级文件系统 — 主程序入口
 * 模拟EXT2文件系统，32MB磁盘，1KB块大小
 * 支持多用户、权限管理、文件和目录操作
 */

#include "fs.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(void)
{
#ifdef _WIN32
    /* 设置控制台编码为UTF-8，解决中文乱码 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    printf("===========================================\n");
    printf("  Linux 二级文件系统 - 基于EXT2设计\n");
    printf("  操作系统课程设计\n");
    printf("===========================================\n");

    /* 初始化文件系统 */
    disk_init();

    /* 初始化文件描述符表 */
    {
        int i;
        for (i = 0; i < MAX_OPEN_FILES; i++) {
            fd_table[i].used = 0;
            fd_table[i].inode_no = 0;
            fd_table[i].offset = 0;
            fd_table[i].mode = 0;
        }
    }

    /* 进入交互式命令行 */
    shell_run();

    /* 清理并退出 */
    /* 关闭所有打开的文件描述符 */
    {
        int i;
        for (i = 0; i < MAX_OPEN_FILES; i++) {
            if (fd_table[i].used) {
                fd_table[i].used = 0;
            }
        }
    }

    disk_close();
    printf("文件系统已关闭，再见!\n");
    return 0;
}
