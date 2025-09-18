CC := gcc

# --- Flags ---
# 统一的调试编译选项
CFLAGS_DEBUG := -g3 -O0 -fno-inline -Wall -Isrc/include -Isrc

# --- Auto-discovery of Core Library Files ---
# [你正确的方式] 自动查找所有核心库的源文件
CORE_SRCS := $(wildcard src/core/*.c)
CORE_OBJS := $(patsubst src/core/%.c, obj/%.o, $(CORE_SRCS))

# --- [新增] Main Application Targets ---
MAIN_SRC := src/main.c
MAIN_OBJ := obj/main.o
MAIN_EXE := out/database

# --- Test Application Targets (动态) ---
TEST ?= test_b_tree_insert
TEST_SRC := test/core/$(TEST).c
TEST_OBJ := obj/$(TEST).o
TARGET_DEBUG := out/$(TEST)_debug

.PHONY: all database debug clean

# --- Build Rules ---

# [新增] 默认目标 'make' 或 'make all' 将构建主程序
all: database

# [新增] 构建主程序的规则
database: $(MAIN_EXE)

# [新增] 链接主程序的规则
$(MAIN_EXE): $(MAIN_OBJ) $(CORE_OBJS)
	@echo "==> Linking Main Application: $@"
	@mkdir -p out
	$(CC) $(CFLAGS_DEBUG) $^ -o $@

# 构建测试程序的规则
debug: $(TARGET_DEBUG)

# 链接测试程序的规则
$(TARGET_DEBUG): $(TEST_OBJ) $(CORE_OBJS)
	@echo "==> Linking Debug Target: $@"
	@mkdir -p out
	$(CC) $(CFLAGS_DEBUG) $^ -o $@

# --- Compilation Rules ---

# 编译核心库文件的通用规则
obj/%.o: src/core/%.c
	@echo "==> Compiling Core: $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

# 编译测试用例的通用规则
obj/%.o: test/core/%.c
	@echo "==> Compiling Test: $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

# [新增] 编译主程序源文件的特定规则
$(MAIN_OBJ): $(MAIN_SRC)
	@echo "==> Compiling Main: $<..."
	@mkdir -p obj
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

# --- Cleanup ---
clean:
	@echo "==> Cleaning up..."
	@rm -rf out obj
	@rm -f table_* # 同时也删除数据库文件