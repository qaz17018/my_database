# 编译器定义
CC := gcc

# 编译选项
# CFLAGS 是通用选项
# -Wall 会显示所有警告，帮助你写出更健壮的代码
# -Isrc/include 告诉编译器去哪里找头文件
CFLAGS := -Wall -Isrc/include
# DEBUG_FLAGS 是专门用于Debug的选项
# -g 会在可执行文件中加入调试信息，是gdb调试的必需品
# -O0 关闭所有优化，确保代码执行顺序和源码一致，方便调试
DEBUG_FLAGS := -g -O0

# ----------------- 源代码文件定义 -----------------
# CORE_SRCS 列出了所有核心逻辑的.c文件
CORE_SRCS := $(wildcard src/core/*.c)
# CORE_OBJS 会自动根据.c文件生成对应的.o（对象文件）路径
CORE_OBJS := $(patsubst src/core/%.c, obj/%.o, $(CORE_SRCS))

# ----------------- 测试目标定义 -----------------
# TEST_TARGETS 定义了我们有多少个测试程序
TEST_TARGETS := test_leaf_split test_b_tree_insert

# --- 编译规则 ---

# "all" 是一个伪目标，作为默认的构建入口
# 输入 `make` 或 `make all` 就会执行这里的命令
.PHONY: all
all: $(TEST_TARGETS)

# "test_b_tree_insert" 目标的生成规则
test_b_tree_insert: obj/test_b_tree_insert.o $(CORE_OBJS)
	@echo "Linking test_b_tree_insert..."
	@mkdir -p out
	$(CC) $(CFLAGS) $^ -o out/$@

# "test_leaf_split" 目标的生成规则
test_leaf_split: obj/test_leaf_split.o $(CORE_OBJS)
	@echo "Linking test_leaf_split..."
	@mkdir -p out
	$(CC) $(CFLAGS) $^ -o out/$@

# --- Debug 版本的编译规则 ---
# 我们为每个测试目标创建一个对应的debug版本
# 例如 `make debug_test_b_tree_insert`
debug_%: obj/debug_%.o $(CORE_OBJS)
	@echo "Linking DEBUG version of $*..."
	@mkdir -p out
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $^ -o out/$@

# --- 通用规则 ---
# 这是如何从一个 .c 文件编译成一个 .o 文件的通用规则
# $< 代表依赖文件中的第一个（即.c文件）
# $@ 代表目标文件（即.o文件）
# `mkdir -p obj` 确保obj目录存在
obj/%.o: src/core/%.c
	@echo "Compiling $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: test/core/%.c
	@echo "Compiling test case $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

# debug版本的 .o 文件编译规则，它会额外加上 DEBUG_FLAGS
obj/debug_%.o: test/core/%.c
	@echo "Compiling DEBUG test case $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -c $< -o $@

# "clean" 规则，用于清理所有生成的文件
.PHONY: clean
clean:
	@echo "Cleaning up..."
	@rm -rf out obj