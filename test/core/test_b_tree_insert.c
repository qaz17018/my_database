#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/include/buffer_pool.h"
#include "../../src/include/b_tree.h"
#include "../../src/include/leaf_page.h" // 需要引入leaf_page来获取MAX_LEAF_RECORDS
#include "../../src/include/internal_page.h"

int main()
{
    uint32_t space_id = 5;

    printf("--- Phase 1: Inserting data and letting B-Tree auto-create ---\n");
    buf_init();

    // 我们不再手动创建，让第一次insert自动创建
    // b_tree_create(space_id);

    int trigger_count = MAX_LEAF_RECORDS + 1;
    printf("Will insert %d records to trigger leaf split and root update.\n\n", trigger_count);

    for (int i = 0; i < trigger_count; i++)
    {
        Row row = {.id = i + 1};
        printf("Inserting row %u...\n", row.id);
        if (b_tree_insert(space_id, &row) != 0)
        {
            fprintf(stderr, "Insertion failed at row %u\n", row.id);
            return -1;
        }
    }

    printf("\n--- Phase 2: Flushing and verifying ---\n");
    buf_flush_all();

    buf_init(); // 清空缓存池
    BTreeMeta *meta = (BTreeMeta *)buf_get_frame(space_id, 0)->data;

    printf("\nAfter split, final root is page: %u\n", meta->root_page_no);
    assert(meta->root_page_no != 1);

    BufferFrame *root_frame = buf_get_frame(space_id, meta->root_page_no);
    assert(root_frame->page_type == PAGE_TYPE_INTERNAL); // 根现在必须是内节点了

    InternalPage *root = (InternalPage *)root_frame->data;
    printf("Final root has %d entries.\n", root->num_entries);
    assert(root->num_entries > 0);

    printf("\nTest passed: B-Tree creation, leaf split, and root creation were handled correctly!\n");

    return 0;
}