#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "row.h"
#include "b_tree_utils.h"
#include "log_manager.h" // [新增] 引入 log_manager

// [新增] 聲明 page.c 中的緩存函數
void file_cache_init();
void file_cache_flush_all();

int main()
{
    uint32_t space_id = 13; // 使用一個新的 space_id 以確保環境乾淨

    printf("========================================================\n");
    printf("||      Benchmark: 1M Inserts, 500K Sequential Deletes      ||\n");
    printf("========================================================\n\n");

    // [修正] 進行完整的初始化
    remove("table_13.db");          // 清理舊的數據文件
    remove("delete_benchmark.log"); // 清理舊的日誌文件
    buf_init();
    file_cache_init();
    log_manager_init("delete_benchmark.log"); // <-- 關鍵修復：初始化日誌管理器

    // --- Phase 1: 插入100萬條記錄 ---
    const int num_insertions = 4000000;
    printf("--- Phase 1: Inserting %d records ---\n", num_insertions);
    for (int i = 0; i < num_insertions; i++)
    {
        Row row = {.id = i + 1};
        // 為了加快速度，只在關鍵節點打印
        if ((i + 1) % 100000 == 0)
            printf("... %d records inserted.\n", i + 1);
        if (b_tree_insert(space_id, &row) != 0)
        {
            fprintf(stderr, "Insertion failed at row id %u\n", row.id);
            return -1;
        }
    }
    printf("Insertion complete.\n");

    // --- Phase 2: 刪除前的健康檢查 ---
    printf("--- Phase 2: Health Check Before Deletes ---\n");
    b_tree_print_stats(space_id);

    // --- Phase 3: 從 ID=1 開始，連續刪除50萬條記錄 ---
    const int num_deletions = 1500000;
    printf("--- Phase 3: Sequentially deleting first %d records ---\n", num_deletions);

    int *deleted_flags = (int *)calloc(num_insertions + 1, sizeof(int));
    clock_t start_time = clock();

    for (int i = 0; i < num_deletions; i++)
    {
        if ((i + 1) % 50000 == 0)
            printf("... %d records deleted.\n", i + 1);
        uint32_t id_to_delete;
        do
        {
            id_to_delete = (rand() % num_insertions) + 1;
        } while (deleted_flags[id_to_delete]); // 如果已經刪了，就再找一個

        table_delete_row(space_id, id_to_delete);
        deleted_flags[id_to_delete] = 1;
    }

    clock_t end_time = clock();
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("\nFinished %d deletions in %.2f seconds (%.2f deletes/sec).\n\n",
           num_deletions, time_spent, num_deletions / time_spent);

    // --- Phase 4: 刪除後的健康檢查 ---
    printf("--- Phase 4: Health Check After Deletes ---\n");
    b_tree_print_stats(space_id);

    // --- Phase 5: 最終一致性驗證 ---
    printf("--- Phase 5: Verifying data consistency ---\n");
    buf_flush_all();
    buf_init();

    for (int i = 1; i <= num_insertions; i++)
    {
        Row *row = b_tree_search(space_id, i);
        if (deleted_flags[i])
        {
            assert(row == NULL); // 確認已刪除的，真的找不到了
        }
        else
        {
            assert(row != NULL && row->id == i); // 確認沒刪除的，一定能找到
        }
    }
    free(deleted_flags);
    printf("Verification successful! All data is consistent.\n\n");

    printf("=====================================================\n");
    printf("||      Sequential Deletion Benchmark Passed!      ||\n");
    printf("=====================================================\n");

    // [新增] 關閉和清理資源
    log_manager_shutdown();
    file_cache_flush_all();

    return 0;
}