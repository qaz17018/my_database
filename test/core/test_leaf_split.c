#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/core/buffer_pool.h"
#include "../../src/include/leaf_page.h"

int main()
{
    buf_init();
    uint32_t space_id = 4;
    uint32_t page0 = 0, page1 = 0;

    for (int i = 0; i < MAX_LEAF_RECORDS; i++)
    {
        Row row = {.id = i + 100, .name = "X"};
        leaf_page_insert_or_split(space_id, page0, &row, &page1);
    }

    Row extra = {.id = 100 + MAX_LEAF_RECORDS, .name = "Split"};
    leaf_page_insert_or_split(space_id, page0, &extra, &page1);

    buf_flush_all();

    buf_init();
    LeafPage *p0 = (LeafPage *)buf_get_page(space_id, page0, PAGE_TYPE_DATA);
    LeafPage *p1 = (LeafPage *)buf_get_page(space_id, page1, PAGE_TYPE_DATA);

    printf("Page0 num_records = %d\n", p0->num_records);
    printf("Page1 num_records = %d\n", p1->num_records);
    printf("Page0 next_leaf = %u\n", p0->next_leaf);

    for (int i = 0; i < p0->num_records; i++)
    {
        printf("P0[%d]: id=%u\n", i, p0->records[i].id);
    }
    for (int i = 0; i < p1->num_records; i++)
    {
        printf("P1[%d]: id=%u\n", i, p1->records[i].id);
    }

    return 0;
}