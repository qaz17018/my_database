#include "leaf_page.h"
#include "buffer_pool.h"
#include <string.h>
#include <stdio.h>

int leaf_page_insert(uint32_t space_id, uint32_t page_no, LeafPage *page, const Row *row)
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

    buf_mark_dirty(space_id, page_no);

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

// [修改] 函数签名匹配头文件
int leaf_page_insert_or_split(uint32_t space_id, uint32_t page_no, const Row *row, uint32_t *out_new_page_no, uint32_t *out_split_key)
{
    LeafPage *page = (LeafPage *)buf_get_page(space_id, page_no, PAGE_TYPE_LEAF);

    // 如果当前页未满，直接插入
    if (page->num_records < MAX_LEAF_RECORDS)
    {
        // 直接调用新的插入函数
        return leaf_page_insert(space_id, page_no, page, row);
    }

    // --- 页分裂逻辑 ---
    printf("Page %u is full, splitting...\n", page_no);

    // 1. 分配一个新页
    uint32_t new_page_no;
    LeafPage *new_page = (LeafPage *)buf_alloc_page(space_id, PAGE_TYPE_LEAF, &new_page_no);
    if (!new_page)
        return -1; // 分配失败
    *out_new_page_no = new_page_no;

    // 2. 将原页的后半部分记录移动到新页
    int mid = MAX_LEAF_RECORDS / 2;
    memcpy(new_page->records, &page->records[mid], sizeof(Row) * (MAX_LEAF_RECORDS - mid));
    new_page->num_records = MAX_LEAF_RECORDS - mid;

    // 3. 更新原页的记录数
    page->num_records = mid;

    // 4. 维护页之间的双向链表关系
    new_page->next_leaf = page->next_leaf; // 新页指向老页的下一个节点
    page->next_leaf = new_page_no;         // 老页指向新页

    // [完成TODO-3] 向上返回分裂键，即新页中的第一条记录的ID
    // 这个键将用于插入到父节点（内节点）中
    *out_split_key = new_page->records[0].id;

    // 5. 判断新记录应该插入到哪个页
    if (row->id < *out_split_key)
    {
        // 插入到原页
        leaf_page_insert(space_id, page_no, page, row);
    }
    else
    {
        // 插入到新页
        leaf_page_insert(space_id, new_page_no, new_page, row);
    }

    // [完成TODO-2] 分裂完成后，原页和新页都已被修改，都应被标记为脏页。
    // leaf_page_insert 内部已经处理了脏页标记，所以这里不需要重复标记。
    // buf_alloc_page 在分配时也已将新页标记为脏页。
    // 此处逻辑已覆盖，无需额外代码。

    return 0;
}