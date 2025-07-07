# 编译器
CC := gcc

# 编译选项
# -Isrc/include 告诉编译器去哪里找头文件
# -Wall 显示所有警告
CFLAGS := -Wall -Isrc/include
# -g 加入调试信息, -O0 关闭优化
DEBUG_FLAGS := -g -O0

# 核心源代码文件
CORE_SRCS := $(wildcard src/core/*.c)
CORE_OBJS := $(patsubst src/core/%.c, obj/%.o, $(CORE_SRCS))

# ----------------- 动态目标定义 -----------------
# 默认的测试文件是 test_b_tree_insert
# 你可以通过命令 `make TEST=test_leaf_split` 来改变它
TEST ?= test_b_tree_insert

# 根据TEST变量，确定源文件和目标文件
TEST_SRC := test/core/$(TEST).c
TEST_OBJ := obj/$(TEST).o
TARGET := out/$(TEST)
TARGET_DEBUG := out/$(TEST)_debug

# --- 主要编译规则 ---

# .PHONY 告诉make这些不是文件名
.PHONY: all debug clean

# `make all` 或 `make` 将会编译一个发布版本
all: $(TARGET)

# `make debug` 将会编译一个调试版本
debug: $(TARGET_DEBUG)

# --- 链接规则 ---

# 链接发布版本
$(TARGET): $(TEST_OBJ) $(CORE_OBJS)
	@echo "Linking release version: $@"
	@mkdir -p out
	$(CC) $(CFLAGS) $^ -o $@

# 链接调试版本
$(TARGET_DEBUG): $(TEST_OBJ) $(CORE_OBJS)
	@echo "Linking debug version: $@"
	@mkdir -p out
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $^ -o $@

# --- 通用编译规则 ---

# 如何从一个 .c 文件编译成一个 .o 文件
obj/%.o: src/core/%.c
	@echo "Compiling $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: test/core/%.c
	@echo "Compiling test case $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	@echo "Cleaning up..."
	@rm -rf out obj