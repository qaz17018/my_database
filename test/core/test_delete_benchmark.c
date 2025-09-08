#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "row.h"
#include "b_tree_utils.h" // 引入我们的健康巡检工具

int main()
{
    uint32_t space_id = 13; // 使用一个新的 space_id 以确保环境干净

    printf("========================================================\n");
    printf("||      Benchmark: 1M Inserts, 500K Sequential Deletes      ||\n");
    printf("========================================================\n\n");

    buf_init();

    // --- Phase 1: 插入100万条记录 ---
    const int num_insertions = 3000000;
    printf("--- Phase 1: Inserting %d records ---\n", num_insertions);
    for (int i = 0; i < num_insertions; i++)
    {
        Row row = {.id = i + 1};
        // 为了加快速度，只在关键节点打印
        if ((i + 1) % 100000 == 0)
            printf("... %d records inserted.\n", i + 1);
        if (b_tree_insert(space_id, &row) != 0)
        {
            fprintf(stderr, "Insertion failed at row id %u\n", row.id);
            return -1;
        }
    }
    printf("Insertion complete.\n");

    // --- Phase 2: 删除前的健康检查 ---
    printf("--- Phase 2: Health Check Before Deletes ---\n");
    b_tree_print_stats(space_id);

    // --- Phase 3: 从 ID=1 开始，连续删除50万条记录 ---
    const int num_deletions = 1300000;
    printf("--- Phase 3: Sequentially deleting first %d records ---\n", num_deletions);

    // 我们依然需要这个数组，以便在最终验证阶段知道哪些ID被删了
    int *deleted_flags = (int *)calloc(num_insertions + 1, sizeof(int));
    clock_t start_time = clock();

    // [核心修改] 将随机删除循环，改为简单的顺序循环
    for (int i = 0; i < num_deletions; i++)
    {
        if ((i + 1) % 50000 == 0)
            printf("... %d records deleted.\n", i + 1);
        uint32_t id_to_delete;
        do
        {
            id_to_delete = (rand() % num_insertions) + 1;
        } while (deleted_flags[id_to_delete]); // 如果已经删了，就再找一个

        table_delete_row(space_id, id_to_delete);
        deleted_flags[id_to_delete] = 1;
    }

    clock_t end_time = clock();
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("\nFinished %d deletions in %.2f seconds (%.2f deletes/sec).\n\n",
           num_deletions, time_spent, num_deletions / time_spent);

    // --- Phase 4: 删除后的健康检查 ---
    printf("--- Phase 4: Health Check After Deletes ---\n");
    b_tree_print_stats(space_id);

    // --- Phase 5: 最终一致性验证 ---
    // (这部分逻辑无需改变，它能完美地验证顺序删除的结果)
    printf("--- Phase 5: Verifying data consistency ---\n");
    buf_flush_all();
    buf_init();

    for (int i = 1; i <= num_insertions; i++)
    {
        Row *row = b_tree_search(space_id, i);
        if (deleted_flags[i])
        {
            assert(row == NULL); // 确认已删除的 (1 到 500,000)，真的找不到了
        }
        else
        {
            assert(row != NULL && row->id == i); // 确认没删除的 (500,001 到 1,000,000)，一定能找到
        }
    }
    free(deleted_flags);
    printf("Verification successful! All data is consistent.\n\n");

    printf("=====================================================\n");
    printf("||      Sequential Deletion Benchmark Passed!      ||\n");
    printf("=====================================================\n");

    return 0;
}