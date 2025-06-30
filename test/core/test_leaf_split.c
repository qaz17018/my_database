#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h> // 引入断言库
#include "../../src/include/buffer_pool.h"
#include "../../src/include/leaf_page.h"

int main()
{
    // --- 环境初始化 ---
    buf_init();
    uint32_t space_id = 4;
    uint32_t root_page_no = 0; // 我们假定0号页是根

    // --- 步骤1: 填满第一页，直到触发分裂 ---
    printf("--- Phase 1: Filling page 0 to trigger a split ---\n");
    uint32_t new_page_no = 0;
    uint32_t split_key = 0;

    // 插入 MAX_LEAF_RECORDS 条记录，填满0号页
    for (int i = 0; i < MAX_LEAF_RECORDS; i++)
    {
        Row row = {.id = i + 100};
        sprintf(row.name, "User%d", i + 100);
        // 调用新的分裂函数，此时还不会分裂
        leaf_page_insert_or_split(space_id, root_page_no, &row, &new_page_no, &split_key);
    }

    // 插入第 MAX_LEAF_RECORDS + 1 条记录，这将触发分裂
    Row extra_row = {.id = MAX_LEAF_RECORDS + 100, .name = "SplitTrigger"};
    leaf_page_insert_or_split(space_id, root_page_no, &extra_row, &new_page_no, &split_key);

    printf("Split triggered. Old page: %u, New page: %u, Split key: %u\n", root_page_no, new_page_no, split_key);

    // --- 步骤2: 将所有脏页写回磁盘 ---
    printf("\n--- Phase 2: Flushing all dirty pages to disk ---\n");
    buf_flush_all();
    printf("Flush complete.\n");

    // --- 步骤3: 重新加载并验证数据 ---
    printf("\n--- Phase 3: Re-loading pages from disk and verifying data ---\n");
    buf_init(); // 清空缓存池，模拟重启
    LeafPage *p0 = (LeafPage *)buf_get_page(space_id, root_page_no, PAGE_TYPE_LEAF);
    LeafPage *p1 = (LeafPage *)buf_get_page(space_id, new_page_no, PAGE_TYPE_LEAF);

    // 验证记录数
    printf("Verifying record counts...\n");
    printf("Page 0 has %d records.\n", p0->num_records);
    printf("Page 1 has %d records.\n", p1->num_records);
    assert(p0->num_records + p1->num_records == MAX_LEAF_RECORDS + 1);

    // [完成TODO-3] 验证页链接关系
    printf("Verifying next_leaf pointers...\n");
    printf("Page 0 next_leaf -> %u\n", p0->next_leaf);
    printf("Page 1 next_leaf -> %u\n", p1->next_leaf);
    assert(p0->next_leaf == new_page_no); // 老页必须指向新页
    assert(p1->next_leaf == 0);           // 新页是最后一页，指向0

    // 验证分裂键
    printf("Verifying split key...\n");
    printf("First key in Page 1 is %u\n", p1->records[0].id);
    assert(p1->records[0].id == split_key);

    // 验证数据有序且不重叠
    printf("Verifying data order across pages...\n");
    uint32_t last_id_p0 = p0->records[p0->num_records - 1].id;
    uint32_t first_id_p1 = p1->records[0].id;
    printf("Last ID in p0: %u, First ID in p1: %u\n", last_id_p0, first_id_p1);
    assert(last_id_p0 < first_id_p1);

    printf("\nAll tests passed successfully!\n");

    return 0;
}