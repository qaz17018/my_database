#include "../../src/include/leaf_page.h"
#include "../../src/core/buffer_pool.h"
#include <string.h>
#include <stdio.h>

int leaf_page_insert(LeafPage *page, const Row *row)
{
    if (page->num_records >= MAX_LEAF_RECORDS)
        return -1;

    int left = 0, right = page->num_records - 1, pos = 0;
    while (left <= right)
    {
        int mid = (left + right) / 2;
        if (page->records[mid].id == row->id)
        {
            return -1;
        }
        else if (page->records[mid].id < row->id)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }
    pos = left;

    for (int i = page->num_records; i > pos; i--)
    {
        page->records[i] = page->records[i - 1];
    }
    page->records[pos] = *row;
    page->num_records++;
    return 0;
}

Row *leaf_page_search(LeafPage *page, uint32_t id)
{
    int left = 0, right = page->num_records - 1;
    while (left <= right)
    {
        int mid = (left + right) / 2;
        if (page->records[mid].id == id)
        {
            return &page->records[mid];
        }
        else if (page->records[mid].id < id)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }
    return NULL;
}

int leaf_page_insert_or_split(uint32_t space_id, uint32_t page_no, const Row *row, uint32_t *out_new_page_no)
{
    static int cnt = 0;
    cnt++;
    LeafPage *page = (LeafPage *)buf_get_page(space_id, page_no, PAGE_TYPE_DATA);
    if (page->num_records < MAX_LEAF_RECORDS)
    {
        return leaf_page_insert(page, row);
    }

    int mid = MAX_LEAF_RECORDS / 2;

    printf("第%d次分裂了\n", cnt);
    LeafPage *new_page = (LeafPage *)buf_alloc_page(space_id, PAGE_TYPE_DATA, out_new_page_no);

    new_page->num_records = MAX_LEAF_RECORDS - mid;
    memcpy(new_page->records, &page->records[mid], sizeof(Row) * new_page->num_records);

    page->num_records = mid;
    page->next_leaf = *out_new_page_no;
    buf_mark_dirty(space_id, page_no);

    if (row->id < new_page->records[0].id)
    {
        return leaf_page_insert(page, row);
    }
    else
    {
        return leaf_page_insert(new_page, row);
    }
}