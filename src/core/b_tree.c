#include "b_tree.h"
#include "buffer_pool.h"
#include "leaf_page.h"
#include "internal_page.h"
#include <stdio.h>
#include <string.h>

static BTreeMeta *get_meta(uint32_t space_id)
{
    return (BTreeMeta*) buf_get_page(space_id, 0, PAGE_TYPE_META);
}

static int b_tree_insert_internal(uint32_t space_id, uint32_t page_no, const Row *row)
{
    PageHeader header;
    void *page_data = buf_get_page(space_id, page_no, 0);

    PageHeader h;
    read_page(space_id, page_no, page_data, &h);

    if (h.page_type == PAGE_TYPE_LEAF)
    {
        printf("Found leaf page %u, attemping to insert.\n", page_no);
        LeafPage *leaf = (LeafPage *)page_data;
        uint32_t new_page_no, split_key;
        return leaf_page_insert_or_split(space_id, page_no, row, &new_page_no, &split_key);
    }
    else if (h.page_type == PAGE_TYPE_INTERNAL)
    {
        printf("Traversing internal page %u.\n", page_no);
        InternalPage *internal = (InternalPage *)page_data;
        uint32_t child_page_no = internal_page_get_child(internal, row->id);
        return b_tree_insert_internal(space_id, child_page_no, row);
    }
    else
    {
        fprintf(stderr, "Errow: Unknow page type %d for page %u\n", h.page_type, page_no);
    }
}

int b_tree_insert(uint32_t space_id, uint32_t root_page_no, const Row *row)
{
    return b_tree_insert_internal(space_id, root_page_no, row);
}