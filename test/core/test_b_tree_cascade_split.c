#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h> // [新增] 引入时间库，用于性能计时
#include "buffer_pool.h"
#include "b_tree.h"
#include "leaf_page.h"
#include "internal_page.h"

int main()
{
    uint32_t space_id = 7; // 使用一个新的space_id，避免与旧测试冲突

    printf("--- Ultimate Stress Test: Million-Scale Insertion Performance ---\n");
    buf_init();

    // [修改] 我们将插入一百万条记录
    const int num_insertions = 1000000;

    printf("Preparing to insert %d records...\n\n", num_insertions);

    // [新增] 记录开始时间
    clock_t start_time = clock();

    for (int i = 0; i < num_insertions; i++)
    {
        Row row = {.id = i + 1};
        // 为了性能，在循环中我们减少打印次数
        if ((i + 1) % 100000 == 0)
        { // 每插入10万条记录，打印一次进度
            printf("... %d records inserted.\n", i + 1);
        }
        if (b_tree_insert(space_id, &row) != 0)
        {
            fprintf(stderr, "Insertion failed at row %u\n", row.id);
            // [新增] 记录结束时间并打印错误信息
            clock_t end_time = clock();
            double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            printf("Test failed after %.2f seconds.\n", time_spent);
            return -1;
        }
    }

    // [新增] 记录插入结束时间
    clock_t insert_end_time = clock();
    double insert_time_spent = (double)(insert_end_time - start_time) / CLOCKS_PER_SEC;
    printf("\nFinished %d insertions in %.2f seconds.\n", num_insertions, insert_time_spent);
    printf("Average inserts per second: %.2f\n\n", num_insertions / insert_time_spent);

    printf("--- Flushing all data to disk... (This may take a moment) ---\n");
    // [新增] 记录刷盘开始时间
    clock_t flush_start_time = clock();
    buf_flush_all();
    // [新增] 记录刷盘结束时间
    clock_t flush_end_time = clock();
    double flush_time_spent = (double)(flush_end_time - flush_start_time) / CLOCKS_PER_SEC;
    printf("Finished flushing in %.2f seconds.\n\n", flush_time_spent);

    printf("--- Verifying final tree structure ---\n");
    buf_init();
    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    assert(meta_frame != NULL);
    BTreeMeta *meta = (BTreeMeta *)meta_frame->data;

    printf("Final root page is: %u\n", meta->root_page_no);
    printf("Total pages used: %u\n\n", meta->total_pages);

    // [修改断言] 我们可以精确地知道最终应该有多少页
    // 1个元数据页 + 插入的100万行数据需要的叶子页 + 过程中产生的内节点页
    // 我们知道总页数肯定比根节点页号大
    assert(meta->total_pages > meta->root_page_no);
    // 并且，一百万条记录，每页256条，至少需要 1000000 / 256 ≈ 3906 个叶子页
    assert(meta->total_pages > 3900);

    printf("=====================================================\n");
    printf("||         Stress Test Passed Successfully!        ||\n");
    printf("=====================================================\n");
    printf("Total time (insert + flush): %.2f seconds.\n", insert_time_spent + flush_time_spent);

    return 0;
}