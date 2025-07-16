#include "secondary_index_page.h"
#include "buffer_pool.h"
#include <string.h>
#include <stdio.h>

// --- 叶子节点函数 ---

int secondary_leaf_page_insert(uint32_t space_id, uint32_t page_no, SecondaryLeafPage *page, const SecondaryLeafEntry *entry)
{
    if (page->num_entries >= MAX_SECONDARY_INTERNAL_ENTRIES)
    {
        return -1; // 页已满
    }

    // 使用 strcmp 进行二分查找，确定插入位置
    int left = 0, right = page->num_entries - 1, pos = 0;
    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(page->entries[mid].key, entry->key);
        if (cmp == 0)
        {
            // 在辅助索引中，键可以重复，我们简单地插在后面
            // 真实地数据库会处理键重复地情况，我们这里简化
            left = mid + 1;
        }
        else if (cmp < 0)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }
    pos = left;

    // 为新条目腾出空间
    for (int i = page->num_entries; i > pos; i--)
    {
        page->entries[i] = page->entries[i - 1];
    }

    // 插入新条目
    page->entries[pos] = *entry;
    page->num_entries++;
    buf_mark_dirty(space_id, page_no);

    return 0;
}

// 在辅助索引的叶子页中查找一个键
// 注意：因为键可能重复，真实的实现会返回一个迭代器或列表
// 这里我们简化，只返回找到的第一个
uint32_t secondary_leaf_page_search(SecondaryLeafPage *page, const char *key)
{
    int left = 0, right = page->num_entries - 1;
    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(page->entries[mid].key, key);
        if (cmp == 0)
        {
            return page->entries[mid].primary_key; // 找到了，返回主键ID
        }
        else if (cmp < 0)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }
    return 0; // 0 表示没找到
}

// 在辅助索引的内节点中查找子节点
uint32_t secondary_internal_page_get_child(SecondaryInternalPage *page, const char *key)
{
    int left = 0, right = page->num_entries - 1;
    int target_idx = -1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        if (strcmp(page->entries[mid].key, key) <= 0)
        {
            target_idx = mid;
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }

    if (target_idx == -1)
    {
        return page->first_child_page_no;
    }
    else
    {
        return page->entries[target_idx].child_page_no;
    }
}