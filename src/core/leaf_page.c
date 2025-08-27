#include "leaf_page.h"
#include "b_tree.h"
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
        int mid = left + (right - left) / 2;
        if (page->records[mid].id == id)
        {
            // [核心修复] 不要返回直接指向缓冲池的指针！
            // 我们在函数内部创建一个静态变量，它是一块安全、持久的内存。
            static Row result_copy;
            // 将找到的数据，从不稳定的缓冲池内存，拷贝到这块安全的内存中。
            result_copy = page->records[mid];
            // 返回这块安全内存的地址。
            return &result_copy;
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
    // 如果没有找到，返回NULL
    return NULL;
}

// [修改] 函数签名匹配头文件
int leaf_page_insert_or_split(uint32_t space_id, uint32_t page_no, const Row *row, uint32_t *out_new_page_no, uint32_t *out_split_key)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;
    LeafPage *page = (LeafPage *)frame->data;
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
    BufferFrame *new_frame = buf_alloc_frame(space_id, PAGE_TYPE_LEAF, &new_page_no);
    if (!new_frame)
        return -1; // 分配失败
    *out_new_page_no = new_page_no;
    LeafPage *new_page = (LeafPage *)new_frame->data;

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
    page = (LeafPage *)frame->data;

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
        return leaf_page_insert(space_id, page_no, page, row);
    }
    else
    {
        // 插入到新页
        return leaf_page_insert(space_id, new_page_no, new_page, row);
    }

    // [完成TODO-2] 分裂完成后，原页和新页都已被修改，都应被标记为脏页。
    // leaf_page_insert 内部已经处理了脏页标记，所以这里不需要重复标记。
    // buf_alloc_page 在分配时也已将新页标记为脏页。
    // 此处逻辑已覆盖，无需额外代码。

    return 0;
}

// [新增] 从一个叶子页中删除一条记录
int leaf_page_delete(uint32_t space_id, uint32_t page_no, LeafPage *page, uint32_t id)
{
    // 1. 使用二分查找，找到要删除的记录的索引(pos)
    int left = 0, right = page->num_records - 1, pos = -1;
    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        if (page->records[mid].id == id)
        {
            pos = mid;
            break;
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

    // 2. 如果没有找到，返回失败
    if (pos == -1)
    {
        return -1; // Record not found
    }

    // 3. 将pos之后的所有记录，向前移动一位，覆盖掉被删除的记录
    for (int i = pos; i < page->num_records - 1; i++)
    {
        page->records[i] = page->records[i + 1];
    }

    // 4. 更新记录总数
    page->num_records--;

    // 5. 将页面标记为“脏”，以便写回磁盘
    buf_mark_dirty(space_id, page_no);

    // TODO: 在这里检查页面是否因为删除而变得“欠载”(underflow)
    // 如果 page->num_records < MAX_LEAF_RECORDS / 2，就需要进行合并或重分配
    // 这是我们下一步要处理的复杂情况

    return 0; // 删除成功
}