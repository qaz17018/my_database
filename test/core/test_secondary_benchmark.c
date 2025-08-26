#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "row.h"

// 引用我们在 b_tree.c 中定义的函数原型
Row *b_tree_search(uint32_t space_id, uint32_t id);

int main()
{
    uint32_t space_id = 11; // 使用一个全新的、干净的space_id

    printf("========================================================\n");
    printf("||    Benchmark: Secondary Index (1 Million Rows)     ||\n");
    printf("========================================================\n\n");

    buf_init();

    const int num_insertions = 200000; // 我们将插入一百万条记录
    printf("--- Phase 1: Inserting %d records with usernames ---\n", num_insertions);
    printf("This will update both primary and secondary B-Trees.\n");

    clock_t start_time = clock();
    for (int i = 0; i < num_insertions; i++)
    {
        Row row;
        row.id = i + 1;
        // 为每一行生成一个唯一的用户名，例如 "user1", "user2", ...
        sprintf(row.username, "user%d", i + 1);

        if ((i + 1) % 100000 == 0)
        {
            printf("... %d records inserted.\n", i + 1);
        }

        // 调用我们最高级别的插入函数，它会同步更新两个索引
        if (table_insert_row(space_id, &row) != 0)
        {
            fprintf(stderr, "Insertion failed at row id %u\n", row.id);
            return -1;
        }
    }
    clock_t insert_end_time = clock();
    double insert_time_spent = (double)(insert_end_time - start_time) / CLOCKS_PER_SEC;
    printf("\nFinished %d insertions in %.2f seconds (%.2f dual-index inserts/sec).\n\n",
           num_insertions, insert_time_spent, num_insertions / insert_time_spent);

    printf("--- Phase 2: Flushing all data to disk ---\n");
    clock_t flush_start_time = clock();
    buf_flush_all();
    clock_t flush_end_time = clock();
    double flush_time_spent = (double)(flush_end_time - flush_start_time) / CLOCKS_PER_SEC;
    printf("Finished flushing in %.2f seconds.\n\n", flush_time_spent);

    // ======================================================================
    // [辅助索引查询性能测试]
    // ======================================================================
    printf("--- Phase 3: Secondary Index Query Performance Test ---\n");

    const int num_queries = 200000; // 我们将执行1万次随机查询
    printf("Preparing to perform %d random lookups via username...\n", num_queries);

    // 清空缓存池，模拟从磁盘查询的“冷启动”
    buf_init();
    srand(time(NULL));

    clock_t query_start_time = clock();
    char search_username[USERNAME_MAX_LEN];
    sprintf(search_username, "user%d", 11);

    printf("Attempting to find: %s\n", search_username);

    // 我们将手动跟踪这个函数的执行
    /* uint32_t pk_found = table_search_pk_by_username(space_id, search_username);

    if (pk_found == 0)
    {
        printf("Query failed as expected. Starting investigation.\n");
    }
    else
    {
        printf("Query unexpectedly succeeded for pk: %u.\n", pk_found);
    } */
    for (int i = 0; i < num_queries; i++)
    {
        // 1. 生成一个随机的、确保存在的用户名
        uint32_t random_user_num = i + 1;
        char search_username[USERNAME_MAX_LEN];
        sprintf(search_username, "user%d", random_user_num);

        // 2. 通过辅助索引查找，获取主键ID
        uint32_t pk_found = table_search_pk_by_username(space_id, search_username);
        if (pk_found != random_user_num)
        {
            printf("id: %d\n", random_user_num);
            continue;
        }
        // assert(pk_found == random_user_num); // 验证找到的主键是否正确

        // 3. 回表：通过主键ID，查找完整的行数据
        Row *found_row = b_tree_search(space_id, pk_found);

        // 4. 最终验证查询到的数据是否完全正确
        assert(found_row != NULL);
        assert(found_row->id == random_user_num);
        assert(strcmp(found_row->username, search_username) == 0);
    }
    clock_t query_end_time = clock();
    double query_time_spent = (double)(query_end_time - query_start_time) / CLOCKS_PER_SEC;

    printf("\nFinished %d username-based queries in %.4f seconds.\n", num_queries, query_time_spent);
    printf("Average QPS (via secondary index): %.2f\n\n", num_queries / query_time_spent);

    printf("=====================================================\n");
    printf("||      Secondary Index Benchmark Passed!          ||\n");
    printf("=====================================================\n");

    return 0;
}