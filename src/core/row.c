#include "../include/row.h"
#include <string.h>

int row_page_insert(RowPage *page, const Row *row)
{
    if (page->num_records >= MAX_ROWS_PER_PAGE)
    {
        return -1;
    }
    page->records[page->num_records++] = *row;
    return 0;
}

Row *row_page_find(RowPage *page, uint32_t id)
{
    for (int i = 0; i < page->num_records; i++)
    {
        if (page->records[i].id == id)
        {
            return &page->records[i];
        }
    }
    return NULL;
}