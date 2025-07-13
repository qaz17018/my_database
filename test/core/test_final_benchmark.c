#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "row.h"

// 我们需要引用b_tree.c中定义的查找函数原型
Row *b_tree_search(uint32_t space_id, uint32_t id);

int main()
{
    uint32_t space_id = 9; // 使用一个全新的space_id

    printf("========================================================\n");
    printf("||         Final Benchmark: 5 Million Inserts         ||\n");
    printf("========================================================\n\n");

    buf_init();

    const int num_insertions = 5000;
    printf("--- Phase 1: Inserting %d records ---\n", num_insertions);

    clock_t start_time = clock();
    for (int i = 0; i < num_insertions; i++)
    {
        Row row = {.id = i + 1};
        // 为了极致的性能，我们只在关键节点打印
        if ((i + 1) % 500000 == 0)
        { // 每插入50万条记录，打印一次进度
            printf("... %d records inserted.\n", i + 1);
        }
        if (row.id == 57)
        {
            printf("debugger point");
        }
        if (b_tree_insert(space_id, &row) != 0)
        {
            fprintf(stderr, "Insertion failed at row %u\n", row.id);
            return -1;
        }
    }
    clock_t insert_end_time = clock();
    double insert_time_spent = (double)(insert_end_time - start_time) / CLOCKS_PER_SEC;
    printf("\nFinished %d insertions in %.2f seconds (%.2f inserts/sec).\n\n",
           num_insertions, insert_time_spent, num_insertions / insert_time_spent);

    printf("--- Phase 2: Flushing all data to disk ---\n");
    clock_t flush_start_time = clock();
    buf_flush_all();
    clock_t flush_end_time = clock();
    double flush_time_spent = (double)(flush_end_time - flush_start_time) / CLOCKS_PER_SEC;
    printf("Finished flushing in %.2f seconds.\n\n", flush_time_spent);

    // ======================================================================
    // [全新功能] 查询性能测试
    // ======================================================================
    printf("--- Phase 3: Query Performance Test ---\n");

    const int num_queries = 1000; // 我们将执行1万次随机查询
    printf("Preparing to perform %d random primary key lookups...\n", num_queries);

    // 在查询前，清空一次缓存池，模拟从磁盘查询的“冷启动”场景
    buf_init();
    srand(time(NULL)); // 使用当前时间作为随机数种子

    clock_t query_start_time = clock();
    for (int i = 0; i < num_queries; i++)
    {

        // 生成一个在1到500万之间的随机ID
        uint32_t random_id = (rand() % num_insertions) + 1;

        printf("第%d次循环开始，id=%u\n", i, random_id);

        Row *found_row = b_tree_search(space_id, random_id);

        // 验证查询的正确性
        assert(found_row != NULL);
        assert(found_row->id == random_id);
    }
    clock_t query_end_time = clock();
    double query_time_spent = (double)(query_end_time - query_start_time) / CLOCKS_PER_SEC;

    printf("\nFinished %d queries in %.4f seconds.\n", num_queries, query_time_spent);
    printf("Average queries per second (QPS): %.2f\n\n", num_queries / query_time_spent);

    printf("=====================================================\n");
    printf("||         Final Benchmark Passed!                 ||\n");
    printf("=====================================================\n");

    return 0;
}