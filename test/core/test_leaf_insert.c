#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/core/buffer_pool.h"
#include "../../src/include/leaf_page.h"

int main()
{
    buf_init();
    uint32_t space_id = 3;
    LeafPage *page = (LeafPage *)buf_get_page(space_id, 0, PAGE_TYPE_DATA);

    Row r1 = {.id = 103, .name = "Tom"};
    Row r2 = {.id = 101, .name = "Alice"};
    Row r3 = {.id = 102, .name = "Bob"};

    leaf_page_insert(page, &r1);
    leaf_page_insert(page, &r2);
    leaf_page_insert(page, &r3);

    buf_mark_dirty(space_id, 0);
    buf_flush_all();

    buf_init();
    LeafPage *reload = (LeafPage *)buf_get_page(space_id, 0, PAGE_TYPE_DATA);

    for (int i = 0; i < reload->num_records; i++)
    {
        printf("Row %d: id=%u, name=%s\n", i, reload->records[i].id, reload->records[i].name);
    }

    Row *found = leaf_page_search(reload, 102);
    if (found)
    {
        printf("Found id=102: name=%s\n", found->name);
    }
    else
    {
        printf("Not found\n");
    }

    return 0;
}