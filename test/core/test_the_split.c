#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "leaf_page.h"
#include "internal_page.h"

int main()
{
    uint32_t space_id = 10; // 一个全新的、干净的空间

    printf("--- Debugging Lab: Dissecting the first leaf split ---\n");

    // 我们只插入刚刚好能触发分裂的数量
    // MAX_LEAF_RECORDS 是根据你的 page.h 和 row.h 计算出的，大约是56
    int trigger_count = MAX_LEAF_RECORDS + 1;

    for (int i = 0; i < trigger_count; i++)
    {
        Row row = {.id = i + 100}; // 从100开始，方便观察
        if (b_tree_insert(space_id, &row) != 0)
        {
            fprintf(stderr, "Insertion failed at row %u\n", row.id);
            return -1;
        }
    }

    buf_flush_all();

    printf("\nSplit should have occurred. Now entering verification.\n");

    buf_init();

    Row *row = b_tree_search(space_id, 110);

    printf("finished");

    // 这里我们可以添加断言来验证分裂后的结构
    // ...

    return 0;
}