#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "row.h"
#include "io_stats.h" // 引入我们的统计工具

int main()
{
    printf("========================================================\n");
    printf("||      Benchmark: Multi-Table Read/Write Mix         ||\n");
    printf("========================================================\n\n");

    const int NUM_TABLES = 10;
    const int RECORDS_PER_TABLE = 1000; // 为了初次运行更快，我们先用一个较小的值

    // --- 在开始任何操作前，清空旧的数据库文件 ---
    remove("mydb.data");
    buf_init();

    // --- Phase 1: 写入数据到 10 个不同的表中 ---
    printf("--- Phase 1: Inserting %d records into %d tables ---\n", RECORDS_PER_TABLE, NUM_TABLES);

    io_stats_init();
    clock_t start_time = clock();

    for (int i = 0; i < RECORDS_PER_TABLE; i++)
    {
        for (int j = 0; j < NUM_TABLES; j++)
        {
            // j+1 就是我们的 space_id
            uint32_t current_space_id = j + 1;
            Row row = {.id = i + 1};
            sprintf(row.username, "user%d", i + 1); // 填充一些数据
            if (table_insert_row(current_space_id, &row) != 0)
            {
                fprintf(stderr, "Insertion failed at space %u, row %u\n", current_space_id, row.id);
                return -1;
            }
        }
    }

    clock_t end_time = clock();
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Insertion complete in %.2f seconds.\n", time_spent);
    io_stats_print();

    // --- Phase 2: 从 10 个表中随机读取数据 ---
    printf("\n--- Phase 2: Performing 5000 random reads across tables ---\n");
    const int NUM_QUERIES = 5000;
    srand(time(NULL));

    io_stats_init();
    start_time = clock();

    for (int i = 0; i < NUM_QUERIES; i++)
    {
        uint32_t random_space_id = (rand() % NUM_TABLES) + 1;
        uint32_t random_id = (rand() % RECORDS_PER_TABLE) + 1;

        Row *found_row = b_tree_search(random_space_id, random_id);
        // 这里我们不做验证，只关注性能
    }

    end_time = clock();
    time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Random reads complete in %.2f seconds.\n", time_spent);
    io_stats_print();

    return 0;
}