#include <stdio.h>
#include <string.h>
#include "../../src/include/buffer_pool.h"
#include "../../src/include/row.h"

int main()
{
    buf_init();
    uint32_t space_id = 1;
    RowPage *page = (RowPage *)buf_get_page(space_id, 0, PAGE_TYPE_LEAF);

    Row r1 = {.id = 1001, .name = "Alice"};
    Row r2 = {.id = 1002, .name = "Bob"};

    row_page_insert(page, &r1);
    row_page_insert(page, &r2);

    buf_mark_dirty(space_id, 0);
    buf_flush_all();

    buf_init();
    RowPage *reload = (RowPage *)buf_get_page(space_id, 0, PAGE_TYPE_LEAF);

    Row *found = row_page_find(reload, 1002);
    if (found)
    {
        printf("Found: id=%u, name=%s\n", found->id, found->name);
    }
    else
    {
        printf("Not found\n");
    }
    return 0;
}