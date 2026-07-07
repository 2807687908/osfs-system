CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L -g
LDFLAGS = -lncurses

# 源文件
SRCS    = main.c fs.c inode.c user.c dir.c file.c shell.c trash.c mmap.c journal.c gui.c
OBJS    = $(SRCS:.c=.o)
TARGET  = filesys

# Windows特殊处理
ifeq ($(OS),Windows_NT)
    TARGET = filesys.exe
    CFLAGS += -D_WIN32
endif

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c fs.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) disk.img

run: $(TARGET)
	./$(TARGET)

# 帮助
help:
	@echo "make        - 编译"
	@echo "make run    - 编译并运行"
	@echo "make clean  - 清理所有生成文件"
