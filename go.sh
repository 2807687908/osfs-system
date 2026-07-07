#!/bin/bash
export PATH="/mingw64/bin:/usr/bin:/bin"
export TMP='/c/Users/32198/AppData/Local/Temp'
export TEMP='C:/Users/32198/AppData/Local/Temp'
cd '/c/Users/32198/Desktop/work/1/操作系统课程设计'
rm -f disk.img
/mingw64/bin/gcc -o filesys.exe main.c fs.c inode.c user.c dir.c file.c shell.c -Wall
echo "=== COMPILE EXIT: $? ==="
./filesys.exe < test_commands.txt 2>&1
echo "=== RUN EXIT: $? ==="
