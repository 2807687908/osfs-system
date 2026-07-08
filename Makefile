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
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c fs.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) disk.img

run: $(TARGET)
	./$(TARGET)

# 单元测试
TEST_SRCS = tests/test_fs.c tests/test_dir.c tests/test_file.c
TEST_TARGETS = $(TEST_SRCS:.c=)

tests: $(TEST_TARGETS)

tests/test_fs: tests/test_fs.c fs.o inode.o
	$(CC) $(CFLAGS) -o $@ $^ -lcunit

tests/test_dir: tests/test_dir.c fs.o inode.o dir.o
	$(CC) $(CFLAGS) -o $@ $^ -lcunit

tests/test_file: tests/test_file.c fs.o inode.o dir.o file.o
	$(CC) $(CFLAGS) -o $@ $^ -lcunit

run-tests: tests
	@echo "Running all tests..."
	@for test in $(TEST_TARGETS); do \
		echo "=== Running $$test ==="; \
		./$$test; \
	done

# 帮助
help:
	@echo "make          - 编译"
	@echo "make run      - 编译并运行"
	@echo "make clean    - 清理所有生成文件"
	@echo "make tests    - 编译单元测试"
	@echo "make run-tests - 运行所有单元测试"
