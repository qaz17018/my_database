#include "internal_page.h"
#include "buffer_pool.h"
#include <string.h>

uint32_t internal_page_get_child(InternalPage *page, uint32_t key)
{
    // 使用二分查找在条目中快速定位
    int left = 0, right = page->num_entries - 1;
    int target_entry_idx = -1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        if (page->entries[mid].key <= key)
        {
            // 如果中间键小于等于目标键，说明目标可能在右边
            target_entry_idx = mid;
            left = mid + 1;
        }
        else
        {
            // 否则，目标在左边
            right = mid - 1;
        }
    }

    if (target_entry_idx == -1)
    {
        // 如果所有键都比key大，则应该访问第一个子节点
        return page->first_child_page_no;
    }
    else
    {
        // 否则，访问找到的那个条目指向的子节点
        return page->entries[target_entry_idx].child_page_no;
    }
}

int internal_page_insert(uint32_t space_id, uint32_t page_no, InternalPage *page, uint32_t key, uint32_t child_page_no)
{
    // TODO: 实现内节点的插入逻辑
    // 这个函数将在后续步骤中实现，当我们需要处理叶子节点分裂时
    if (page->num_entries >= MAX_INTERNAL_ENTRIES)
    {
        return -1;
    }

    int pos = 0;
    while (pos < page->num_entries && page->entries[pos].key < key)
    {
        pos++;
    }

    for (int i = page->num_entries; i > pos; i--)
    {
        page->entries[i] = page->entries[i - 1];
    }

    page->entries[pos].key = key;
    page->entries[pos].child_page_no = child_page_no;
    page->num_entries++;

    buf_mark_dirty(space_id, page_no);

    return 0;
}