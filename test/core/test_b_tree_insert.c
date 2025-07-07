#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/include/buffer_pool.h"
#include "../../src/include/b_tree.h"
#include "../../src/include/leaf_page.h"
#include "../../src/include/internal_page.h"

int main()
{
    uint32_t space_id = 5;

    // --- Phase 1: 创建一棵全新的B+树 ---
    printf("--- Phase 1: Creating a new B+Tree ---\n");
    buf_init();
    b_tree_create(space_id);

    // --- Phase 2: 插入数据，直到触发根节点分裂 ---
    // 这个数量需要大于一个叶子页的容量 + 一个内节点的容量
    // 我们先测试一次叶子分裂，看看根节点是否正确更新
    printf("\n--- Phase 2: Inserting data to trigger root split ---\n");
    int trigger_count = MAX_LEAF_RECORDS + 1;
    printf("Will insert %d records to trigger leaf split and update root.\n", trigger_count);

    for (int i = 0; i < trigger_count; i++)
    {
        Row row = {.id = i + 1};
        sprintf(row.name, "User%d", i + 1);
        b_tree_insert(space_id, &row);
    }

    // --- Phase 3: 写回磁盘并验证 ---
    printf("\n--- Phase 3: Flushing and verifying ---\n");
    buf_flush_all();

    buf_init();
    BTreeMeta *meta = (BTreeMeta *)buf_get_page(space_id, 0, PAGE_TYPE_META);
    printf("After split, new root is page: %u\n", meta->root_page_no);
    assert(meta->root_page_no != 1); // 根节点肯定已经不是初始的1号页了

    InternalPage *root = (InternalPage *)buf_get_page(space_id, meta->root_page_no, PAGE_TYPE_INTERNAL);
    printf("New root has %d entries.\n", root->num_entries);
    assert(root->num_entries > 0);

    printf("Test passed: Root split was handled correctly!\n");

    return 0;
}