#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "row.h"

int main()
{
    uint32_t space_id = 14; // 使用一个全新的 space_id

    printf("========================================================\n");
    printf("||       Ultimate Benchmark: 2M Inserts, 2M Updates     ||\n");
    printf("========================================================\n\n");

    buf_init();

    // --- Phase 1: 插入200万条记录 ---
    const int num_records = 2000000;
    printf("--- Phase 1: Inserting %d records ---\n", num_records);
    clock_t start_time = clock();
    for (int i = 0; i < num_records; i++)
    {
        Row row;
        row.id = i + 1;
        sprintf(row.username, "user%d", i + 1);
        sprintf(row.email, "user%d@example.com", i + 1);
        if ((i + 1) % 200000 == 0)
            printf("... %d records inserted.\n", i + 1);
        if (table_insert_row(space_id, &row) != 0)
        {
            fprintf(stderr, "Insertion failed at row id %u\n", row.id);
            return -1;
        }
    }
    clock_t end_time = clock();
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Finished insertions in %.2f seconds (%.2f inserts/sec).\n\n", time_spent, num_records / time_spent);

    // --- Phase 2: 更新前的健康检查 ---
    printf("--- Phase 2: Health Check Before Updates ---\n");
    b_tree_print_stats(space_id);           // 主B+树体检
    secondary_b_tree_print_stats(space_id); // [新增] 辅助索引体检

    // --- Phase 3: 更新全部200万条记录的username ---
    printf("--- Phase 2: Updating all %d records ---\n", num_records);
    start_time = clock();
    for (int i = 0; i < num_records; i++)
    {
        Row updated_row;
        updated_row.id = i + 1;
        // 创建一个新的、与旧的完全不同的username
        sprintf(updated_row.username, "updated_user%d", i + 1);
        sprintf(updated_row.email, "updated%d@example.com", i + 1);
        if ((i + 1) % 200000 == 0)
            printf("... %d records updated.\n", i + 1);
        if (table_update_row(space_id, &updated_row) != 0)
        {
            fprintf(stderr, "Update failed at row id %u\n", updated_row.id);
            return -1;
        }
    }
    end_time = clock();
    time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Finished updates in %.2f seconds (%.2f updates/sec).\n\n", time_spent, num_records / time_spent);

    // --- Phase 4: 更新后的健康检查 ---
    printf("--- Phase 4: Health Check After Updates ---\n");
    b_tree_print_stats(space_id);           // 主B+树体检
    secondary_b_tree_print_stats(space_id); // [新增] 辅助索引体检

    // --- Phase 5: 随机验证100万条数据的正确性 ---
    const int num_queries = 1000000;
    printf("--- Phase 3: Verifying %d random records ---\n", num_queries);
    buf_flush_all();
    buf_init();
    srand(time(NULL));

    start_time = clock();
    for (int i = 0; i < num_queries; i++)
    {
        if ((i + 1) % 100000 == 0)
            printf("... %d records verified.\n", i + 1);

        uint32_t random_id = (rand() % num_records) + 1;

        char expected_username[USERNAME_MAX_LEN];
        sprintf(expected_username, "updated_user%d", random_id);

        // 1. 验证通过新username能找到正确的ID
        uint32_t pk_found = table_search_pk_by_username(space_id, expected_username);
        assert(pk_found == random_id);

        // 2. 验证通过旧username找不到任何东西 (需要辅助索引删除功能正常工作)
        char old_username[USERNAME_MAX_LEN];
        sprintf(old_username, "user%d", random_id);
        pk_found = table_search_pk_by_username(space_id, old_username);
        assert(pk_found == 0); // 必须为0，表示未找到

        // 3. 验证回表查询的数据完全正确
        Row *row = b_tree_search(space_id, random_id);
        assert(row != NULL);
        assert(row->id == random_id);
        assert(strcmp(row->username, expected_username) == 0);
    }
    end_time = clock();
    time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Verification successful in %.2f seconds!\n\n", time_spent);

    printf("=====================================================\n");
    printf("||         Ultimate Benchmark Passed!              ||\n");
    printf("=====================================================\n");

    return 0;
}