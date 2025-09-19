#include <stdio.h>
#include <stdlib.h>
#include <time.h> // 引入计时库
#include "buffer_pool.h"
#include "b_tree.h"
#include "io_stats.h"

// 声明我们在 page.c 中创建的缓存函数
void file_cache_init();
void file_cache_flush_all();

int main()
{
    printf("========================================================\n");
    printf("||     STRESS TEST: 1 Million Record LRU Pollution    ||\n");
    printf("========================================================\n\n");

    const uint32_t HOT_TABLE_ID = 1;
    const uint32_t COLD_TABLE_ID = 2;

    // 清理旧文件
    remove("table_1.db");
    remove("table_2.db");

    buf_init();
    file_cache_init();

    // --- Phase 1: 创建并访问 "热" 数据 ---
    printf("--- Phase 1: Accessing a few 'hot' pages in table %u ---\n", HOT_TABLE_ID);
    io_stats_init();

    for (int i = 0; i < 3; i++)
    {
        Row r = {.id = i * 1000};
        sprintf(r.username, "hot_user%d", i);
        table_insert_row(HOT_TABLE_ID, &r);
    }
    b_tree_search(HOT_TABLE_ID, 0);
    b_tree_search(HOT_TABLE_ID, 1000);
    b_tree_search(HOT_TABLE_ID, 2000);
    printf("Hot pages accessed and warmed up in cache.\n");
    io_stats_print();

    // --- Phase 2: 百万级记录插入，污染缓存 ---
    const int POLLUTION_RECORDS = 1000000; // <<<--- 提升到百万级别！
    printf("\n--- Phase 2: Polluting cache with %d records from table %u ---\n", POLLUTION_RECORDS, COLD_TABLE_ID);
    printf("This will take some time...\n");

    clock_t start_time = clock(); // 开始计时

    for (int i = 0; i < POLLUTION_RECORDS; i++)
    {
        Row r = {.id = i};
        sprintf(r.username, "cold_user%d", i);
        table_insert_row(COLD_TABLE_ID, &r);

        // --- [新增] 进度条 ---
        if ((i + 1) % 100000 == 0)
        {
            printf("... %d records inserted.\n", i + 1);
        }
    }

    clock_t end_time = clock(); // 结束计时
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Cache pollution complete in %.2f seconds.\n", time_spent);

    // --- Phase 3: 重新访问 "热" 数据，验证缓存是否依然有效 ---
    printf("\n--- Phase 3: Re-accessing 'hot' pages to verify cache integrity ---\n");
    io_stats_init();

    Row *r_found = b_tree_search(HOT_TABLE_ID, 1000);
    if (!r_found)
        printf("Error: Hot record 1000 not found!\n");

    printf("Re-access complete.\n");
    // 最终的审判：fread_calls 是否依然为 0？
    io_stats_print();

    file_cache_flush_all();
    return 0;
}