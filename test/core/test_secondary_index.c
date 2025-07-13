#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "row.h"

// 在测试文件中，我们需要引用我们刚在.c文件中实现的函数原型
// 这是C语言模块化编程的一种方式
Row *b_tree_search(uint32_t space_id, uint32_t id);

int main()
{
    uint32_t space_id = 8; // 使用一个全新的space_id

    printf("--- Test Case: Secondary Index Insertion and Lookup ---\n");
    buf_init();

    // --- 步骤 1: 插入几条带姓名的数据 ---
    printf("\n--- Phase 1: Inserting rows with usernames ---\n");

    Row r1 = {.id = 101, .username = "alice"};
    Row r2 = {.id = 102, .username = "bob"};
    Row r3 = {.id = 103, .username = "charlie"};

    assert(table_insert_row(space_id, &r1) == 0);
    assert(table_insert_row(space_id, &r2) == 0);
    assert(table_insert_row(space_id, &r3) == 0);

    printf("Insertion complete.\n");

    // --- 步骤 2: 通过辅助索引查找 ---
    printf("\n--- Phase 2: Searching by username 'charlie' ---\n");

    // 第一步：通过username找到主键ID
    uint32_t pk_found = table_search_pk_by_username(space_id, "charlie");
    printf("Found primary key for 'charlie': %u\n", pk_found);
    assert(pk_found == 103);

    // --- 步骤 3: 回表查询 ---
    printf("\n--- Phase 3: Looking up full row using primary key %u ---\n", pk_found);

    // 第二步：拿着主键ID，回到主B+树查找完整的行数据
    Row *row_found = b_tree_search(space_id, pk_found);
    assert(row_found != NULL);

    printf("Lookup successful! Found row: id=%u, username=%s\n", row_found->id, row_found->username);
    assert(row_found->id == 103);
    assert(strcmp(row_found->username, "charlie") == 0);

    // --- 步骤 4: 刷盘并再次验证 ---
    printf("\n--- Phase 4: Flushing to disk and re-verifying ---\n");
    buf_flush_all();
    buf_init(); // 清空缓存池，模拟重启

    uint32_t pk_after_flush = table_search_pk_by_username(space_id, "alice");
    printf("Found primary key for 'alice' after flush: %u\n", pk_after_flush);
    assert(pk_after_flush == 101);

    printf("\n========================================================\n");
    printf("||   Secondary Index Test Passed Successfully!        ||\n");
    printf("========================================================\n");

    return 0;
}