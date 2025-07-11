#include <stdio.h>
#include <string.h>
#include "b_tree.h" // 需要引入它来获取Meta定义
#include "buffer_pool.h"
#include "secondary_index_page.h"

// 声明内部函数
static int secondary_b_tree_insert_internal(uint32_t space_id, uint32_t page_no, const SecondaryLeafEntry *entry,
                                            char *out_split_key, uint32_t *out_new_page_no);

// (这个文件的其余函数是为了辅助索引服务的，逻辑与b_tree.c类似，但使用字符串比较)

// 内部函数：向辅助索引内节点插入或分裂
static int secondary_internal_page_insert_or_split(uint32_t space_id, uint32_t page_no, const char *key, uint32_t child_page_no,
                                                   char *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;
    SecondaryInternalPage *page = (SecondaryInternalPage *)frame->data;

    // TODO: 实现内节点的分裂逻辑 (暂时简化，先假设不分裂)
    if (page->num_entries >= MAX_SECONDARY_INTERNAL_ENTRIES)
    {
        printf("PANIC: Secondary internal page split not implemented yet.\n");
        return -1;
    }

    // 找到插入位置
    int pos = 0;
    while (pos < page->num_entries && strcmp(page->entries[pos].key, key) < 0)
    {
        pos++;
    }
    for (int i = page->num_entries; i > pos; i--)
    {
        page->entries[i] = page->entries[i - 1];
    }
    strcpy(page->entries[pos].key, key);
    page->entries[pos].child_page_no = child_page_no;
    page->num_entries++;
    buf_mark_dirty(space_id, page_no);
    return 0;
}

// 内部函数：向辅助索引叶子节点插入或分裂
static int secondary_leaf_page_insert_or_split(uint32_t space_id, uint32_t page_no, const SecondaryLeafEntry *entry,
                                               char *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;
    SecondaryLeafPage *page = (SecondaryLeafPage *)frame->data;

    if (page->num_entries < MAX_SECONDARY_LEAF_ENTRIES)
    {
        return secondary_leaf_page_insert(space_id, page_no, page, entry);
    }

    // TODO: 实现叶子节点的分裂逻辑 (暂时简化)
    printf("PANIC: Secondary leaf page split not implemented yet.\n");
    return -1;
}

// 主入口：向辅助B+树中插入一条记录
int secondary_b_tree_insert(uint32_t space_id, const SecondaryLeafEntry *entry)
{
    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    if (!meta_frame)
        return -1;
    BTreeMeta *meta = (BTreeMeta *)meta_frame->data;

    // 如果辅助索引还未创建
    if (meta->username_idx_root_page_no == 0)
    {
        uint32_t root_page_no;
        buf_alloc_frame(space_id, PAGE_TYPE_SECONDARY_LEAF, &root_page_no);
        meta_frame = buf_get_frame(space_id, 0);
        meta = (BTreeMeta *)meta_frame->data;
        meta->username_idx_root_page_no = root_page_no;
        meta->total_pages++;
        buf_mark_dirty(space_id, 0);
    }

    uint32_t root_page_no = meta->username_idx_root_page_no;
    char split_key[USERNAME_MAX_LEN] = {0};
    uint32_t new_page_no = 0;

    secondary_b_tree_insert_internal(space_id, root_page_no, entry, split_key, &new_page_no);

    // TODO: 处理根节点分裂 (暂时简化)
    if (split_key[0] != '\0')
    {
        printf("PANIC: Secondary root page split not implemented yet.\n");
    }

    return 0;
}

static int secondary_b_tree_insert_internal(uint32_t space_id, uint32_t page_no, const SecondaryLeafEntry *entry,
                                            char *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;

    // 根据页面类型，决定如何处理
    if (frame->page_type == PAGE_TYPE_SECONDARY_LEAF)
    {
        return secondary_leaf_page_insert_or_split(space_id, page_no, entry, out_split_key, out_new_page_no);
    }

    if (frame->page_type == PAGE_TYPE_SECONDARY_INTERNAL)
    {
        SecondaryInternalPage *page = (SecondaryInternalPage *)frame->data;
        uint32_t child_page_no = secondary_internal_page_get_child(page, entry->key);
        secondary_b_tree_insert_internal(space_id, child_page_no, entry, out_split_key, out_new_page_no);

        // TODO: 处理子节点分裂 (暂时简化)
        if (out_split_key[0] != '\0')
        {
            printf("PANIC: Secondary internal page needs to handle child split.\n");
        }
        return 0;
    }

    return -1; // 未知页面类型
}