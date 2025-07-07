#include "b_tree.h"
#include "buffer_pool.h"
#include "leaf_page.h"
#include "internal_page.h"
#include <stdio.h>
#include <string.h>

static BTreeMeta *get_meta(uint32_t space_id)
{
    // 0号页永远是元数据页
    return (BTreeMeta *)buf_get_page(space_id, 0, PAGE_TYPE_META);
}

int b_tree_create(uint32_t space_id)
{
    BTreeMeta *meta = get_meta(space_id);
    uint32_t root_page_no;
    buf_alloc_page(space_id, PAGE_TYPE_LEAF, &root_page_no);

    meta->root_page_no = root_page_no;
    meta->total_pages = 2; // meta page + root page

    buf_mark_dirty(space_id, 0);
    buf_flush_all();
    printf("B+Tree created for sppace %u. Root is page %u\n", space_id, root_page_no);
    return 0;
}

static int b_tree_insert_internal(uint32_t space_id, uint32_t page_no, const Row *row,
                                  uint32_t *out_split_key, uint32_t *out_new_page_no)
{
    void *page_data = buf_get_page(space_id, page_no, 0);
    PageHeader h;
    read_page(space_id, page_no, page_data, &h);

    if (h.page_type == PAGE_TYPE_LEAF)
    {
        printf("Found leaf page %u, attemping to insert.\n", page_no);
        LeafPage *leaf = (LeafPage *)page_data;
        return leaf_page_insert_or_split(space_id, page_no, row, out_new_page_no, out_split_key);
    }

    InternalPage *internal = (InternalPage *)page_data;
    uint32_t child_page_no = internal_page_get_child(internal, row->id);

    int result = b_tree_insert_internal(space_id, child_page_no, row, out_split_key, out_new_page_no);

    if (*out_split_key == 0)
    {
        return 0;
    }

    uint32_t new_key = *out_split_key;
    uint32_t new_child_page = *out_new_page_no;

    *out_split_key = 0;
    *out_new_page_no = 0;

    // 尝试将分裂键插入到当前内节点
    if (internal->num_entries < MAX_INTERNAL_ENTRIES)
    {
        return internal_page_insert(space_id, page_no, internal, new_key, new_child_page);
    }
    else
    {
        // [TODO] 当前内节点也满了，需要分裂内节点
        // 这是下一阶段最核心的任务
        printf("PANIC: Internal page %u is full, needs splitting! (Not implemented yet)\n", page_no);
        return -1;
    }
}

int b_tree_insert(uint32_t space_id, const Row *row)
{
    BTreeMeta *meta = get_meta(space_id);
    uint32_t root_page_no = meta->root_page_no;

    uint32_t split_key = 0;
    uint32_t new_page_no = 0;

    b_tree_insert_internal(space_id, root_page_no, row, &split_key, &new_page_no);

    if (split_key > 0)
    {
        printf("Root page %u has split!\n", root_page_no);
        uint32_t new_root_page_no;
        InternalPage *new_root = (InternalPage *)buf_alloc_page(space_id, PAGE_TYPE_INTERNAL, &new_root_page_no);
        new_root->first_child_page_no = root_page_no;
        new_root->entries[0].key = split_key;
        new_root->entries[0].child_page_no = new_page_no;
        new_root->num_entries = 1;

        meta->root_page_no = new_root_page_no;
        new_root->num_entries = 1;

        meta->root_page_no = new_root_page_no;
        buf_mark_dirty(space_id, 0);
        printf("New root is page %u. Tree height increased.\n", new_root_page_no);
    }
    return 0;
}