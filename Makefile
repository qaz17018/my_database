CC := gcc

# [修改] CFLAGS 和 DEBUG_FLAGS 合并，并使用最高级别的调试信息
# -g3: 包含最详细的调试信息，包括宏定义
# -O0: 彻底关闭所有优化
# -fno-inline: 禁止内联函数，确保你能单步进入每一个函数
# -Wall: 显示所有警告
CFLAGS_DEBUG := -g3 -O0 -fno-inline -Wall -Isrc/include

CORE_SRCS := $(wildcard src/core/*.c)
CORE_OBJS := $(patsubst src/core/%.c, obj/%.o, $(CORE_SRCS))

# 动态目标
TEST ?= test_b_tree_insert
TEST_SRC := test/core/$(TEST).c
TEST_OBJ := obj/$(TEST).o
TARGET_DEBUG := out/$(TEST)_debug

.PHONY: debug clean

# `make debug` 是我们唯一的构建命令
debug: $(TARGET_DEBUG)

# 链接规则
$(TARGET_DEBUG): $(TEST_OBJ) $(CORE_OBJS)
	@echo "Linking debug version: $@"
	@mkdir -p out
	$(CC) $(CFLAGS_DEBUG) $^ -o $@

# 编译规则 for .c -> .o
obj/%.o: src/core/%.c
	@echo "Compiling $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

obj/%.o: test/core/%.c
	@echo "Compiling test case $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

clean:
	@echo "Cleaning up..."
	@rm -rf out obj