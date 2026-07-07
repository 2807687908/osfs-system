#!/bin/bash
export TMP='/c/Users/32198/AppData/Local/Temp'
export TEMP='C:/Users/32198/AppData/Local/Temp'
cd '/c/Users/32198/Desktop/work/1/操作系统课程设计'
/mingw64/bin/gcc -o fs_test.exe main.c fs.c inode.c user.c dir.c file.c shell.c -Wall 2>&1
echo "EXIT=$?"
