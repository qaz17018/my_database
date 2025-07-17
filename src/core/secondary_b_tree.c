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

    // 如果未满，直接插入
    if (page->num_entries < MAX_SECONDARY_INTERNAL_ENTRIES)
    {
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

    // --- 内节点分裂 ---
    SecondaryInternalEntry temp_entries[MAX_SECONDARY_INTERNAL_ENTRIES + 1];
    int pos = 0;
    while (pos < MAX_SECONDARY_INTERNAL_ENTRIES && strcmp(page->entries[pos].key, key) < 0)
    {
        temp_entries[pos] = page->entries[pos];
        pos++;
    }
    strcpy(temp_entries[pos].key, key);
    temp_entries[pos].child_page_no = child_page_no;
    for (int i = pos; i < MAX_SECONDARY_INTERNAL_ENTRIES; i++)
    {
        temp_entries[i + 1] = page->entries[i];
    }

    int mid_idx = (MAX_SECONDARY_INTERNAL_ENTRIES + 1) / 2;
    strcpy(out_split_key, temp_entries[mid_idx].key);

    uint32_t new_page_no;
    BufferFrame *new_frame = buf_alloc_frame(space_id, PAGE_TYPE_SECONDARY_INTERNAL, &new_page_no);
    if (!new_frame)
        return -1;
    *out_new_page_no = new_page_no;
    SecondaryInternalPage *new_page = (SecondaryInternalPage *)new_frame->data;

    ((BTreeMeta *)buf_get_frame(space_id, 0)->data)->total_pages++;
    buf_mark_dirty(space_id, 0);

    page->num_entries = mid_idx;
    new_page->num_entries = MAX_SECONDARY_INTERNAL_ENTRIES - mid_idx;
    new_page->first_child_page_no = temp_entries[mid_idx].child_page_no;
    memcpy(new_page->entries, &temp_entries[mid_idx + 1], new_page->num_entries * sizeof(SecondaryInternalEntry));

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
    uint32_t new_page_no;
    BufferFrame *new_frame = buf_alloc_frame(space_id, PAGE_TYPE_SECONDARY_LEAF, &new_page_no);
    if (!new_frame)
        return -1;
    *out_new_page_no = new_page_no;
    SecondaryLeafPage *new_page = (SecondaryLeafPage *)new_frame->data;

    // [核心修复-1] 在成功分配新页后，更新元数据
    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    if (meta_frame)
    {
        BTreeMeta *meta = (BTreeMeta *)meta_frame->data;
        meta->total_pages++;
        buf_mark_dirty(space_id, 0);
    }

    frame = buf_get_frame(space_id, page_no); // 重新获取，因为buf_alloc_frame可能导致其被淘汰
    if (!frame)
        return -1;
    page = (SecondaryLeafPage *)frame->data;

    int mid = MAX_SECONDARY_LEAF_ENTRIES / 2;
    memcpy(new_page->entries, &page->entries[mid], sizeof(SecondaryLeafEntry) * (MAX_SECONDARY_LEAF_ENTRIES - mid));
    new_page->num_entries = MAX_SECONDARY_LEAF_ENTRIES - mid;
    page->num_entries = mid;
    new_page->next_leaf = page->next_leaf;

    page->next_leaf = new_page_no;

    *out_split_key = new_page->entries[0].key;

    if (strcpy(entry->key, out_split_key) > 0)
    {
        return secondary_leaf_page_insert(space_id, page_no, page, entry);
    }
    else
    {
        return secondary_leaf_page_insert(space_id, new_page_no, new_page, entry);
    }

    return 0;
}

// 主入口：向辅助B+树中插入一条记录
// --- B+树插入主逻辑 ---
int secondary_b_tree_insert(uint32_t space_id, const SecondaryLeafEntry *entry)
{
    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    if (!meta_frame)
        return -1;
    BTreeMeta *meta = (BTreeMeta *)meta_frame->data;

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

    if (split_key[0] != '\0')
    {
        uint32_t old_root_page_no = root_page_no;
        uint32_t new_root_page_no;
        BufferFrame *new_root_frame = buf_alloc_frame(space_id, PAGE_TYPE_SECONDARY_INTERNAL, &new_root_page_no);
        if (!new_root_frame)
            return -1;

        ((BTreeMeta *)buf_get_frame(space_id, 0)->data)->total_pages++;
        buf_mark_dirty(space_id, 0);

        SecondaryInternalPage *new_root = (SecondaryInternalPage *)new_root_frame->data;
        new_root->first_child_page_no = old_root_page_no;
        strcpy(new_root->entries[0].key, split_key);
        new_root->entries[0].child_page_no = new_page_no;
        new_root->num_entries = 1;

        meta = (BTreeMeta *)buf_get_frame(space_id, 0)->data;
        meta->username_idx_root_page_no = new_root_page_no;
        buf_mark_dirty(space_id, 0);
    }
    return 0;
}

static int secondary_b_tree_insert_internal(uint32_t space_id, uint32_t page_no, const SecondaryLeafEntry *entry,
                                            char *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;

    if (frame->page_type == PAGE_TYPE_SECONDARY_LEAF)
    {
        return secondary_leaf_page_insert_or_split(space_id, page_no, entry, out_split_key, out_new_page_no);
    }

    if (frame->page_type == PAGE_TYPE_SECONDARY_INTERNAL)
    {
        SecondaryInternalPage *page = (SecondaryInternalPage *)frame->data;
        uint32_t child_page_no = secondary_internal_page_get_child(page, entry->key);
        secondary_b_tree_insert_internal(space_id, child_page_no, entry, out_split_key, out_new_page_no);

        if (out_split_key[0] != '\0')
        {
            return secondary_internal_page_insert_or_split(space_id, page_no, out_split_key, *out_new_page_no, out_split_key, out_new_page_no);
        }
        return 0;
    }

    return -1;
}

// 声明一个内部递归函数
static uint32_t secondary_b_tree_search_internal(uint32_t space_id, uint32_t page_no, const char *key);

// 顶层的辅助索引查找函数
uint32_t secondary_b_tree_search(uint32_t space_id, const char *key)
{
    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    if (!meta_frame)
        return 0; // 0 代表未找到
    BTreeMeta *meta = (BTreeMeta *)meta_frame->data;

    if (meta->username_idx_root_page_no == 0)
    {
        return 0; // 辅助索引树不存在
    }

    return secondary_b_tree_search_internal(space_id, meta->username_idx_root_page_no, key);
}

// 内部递归查找函数
static uint32_t secondary_b_tree_search_internal(uint32_t space_id, uint32_t page_no, const char *key)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return 0;

    if (frame->page_type == PAGE_TYPE_SECONDARY_LEAF)
    {
        return secondary_leaf_page_search((SecondaryLeafPage *)frame->data, key);
    }

    if (frame->page_type == PAGE_TYPE_SECONDARY_INTERNAL)
    {
        uint32_t child_page_no = secondary_internal_page_get_child((SecondaryInternalPage *)frame->data, key);
        return secondary_b_tree_search_internal(space_id, child_page_no, key);
    }

    return 0; // 未知页面类型或未找到
}