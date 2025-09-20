#include "internal_page.h"
#include <stdio.h>
#include "buffer_pool.h"
#include "b_tree.h"
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
// uint32_t internal_page_get_child(InternalPage *page, uint32_t key)
// {
//     // 从左到右扫描内节点的所有键
//     for (int i = 0; i < page->num_entries; i++)
//     {
//         // 只要找到第一个比我们要找的key大的键
//         if (key < page->entries[i].key)
//         {
//             // 就返回这个键“左边”的那个指针
//             // 如果是第一个键，就返回page的first_child_page_no
//             // 否则，返回前一个条目的child_page_no
//             // 但因为我们的结构，first_child_page_no涵盖了i=0的情况，
//             // 而我们的查询逻辑应该已经保证了key不会小于所有键，
//             // 为了代码的简洁和正确，我们简化为：
//             // 如果key比entries[0].key还小，那应该走first_child_page_no
//             // 所以这个逻辑需要更严谨地表达

//             // 更严谨、清晰的线性扫描逻辑：
//             if (key < page->entries[0].key)
//             {
//                 return page->first_child_page_no;
//             }
//             // 实际上，我们的二分查找的逻辑等价于寻找最后一个小于等于key的键
//             // 让我们用线性方式重新实现这个逻辑
//         }
//     }

//     // 经过反复思考，原二分查找的逻辑目标是正确的，但实现有微小的缺陷。
//     // 我们用一个绝对不会出错的、从后往前的线性扫描来确保正确性。
//     // 从右往左找，找到第一个小于等于key的条目
//     for (int i = page->num_entries - 1; i >= 0; i--)
//     {
//         if (page->entries[i].key <= key)
//         {
//             // 找到了，返回它指向的子节点
//             return page->entries[i].child_page_no;
//         }
//     }

//     // 如果所有键都比key大，说明应该走最左边的那个孩子
//     return page->first_child_page_no;
// }

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

    buf_mark_dirty(space_id, page_no, 0);

    return 0;
}

/* int internal_page_insert_or_split(uint32_t space_id, uint32_t page_no, uint32_t key, uint32_t child_page_no, uint32_t *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;
    InternalPage *page = (InternalPage *)frame->data;

    // Case 1: 内节点未满，直接插入
    if (page->num_entries < MAX_INTERNAL_ENTRIES)
    {
        return internal_page_insert(space_id, page_no, page, key, child_page_no);
    }

    // Case 2: 内节点已满，需要分裂
    printf("Internal page %u is full, splitting...\n", page_no);

    // 1. 创建一个新的内节点页
    uint32_t new_page_no;
    BufferFrame *new_frame = buf_alloc_frame(space_id, PAGE_TYPE_INTERNAL, &new_page_no);
    if (!new_frame)
        return -1;
    *out_new_page_no = new_page_no;
    InternalPage *new_page = (InternalPage *)new_frame->data;

    // [核心修复-2] 在成功分配新页后，更新元数据
    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    if (meta_frame)
    {
        BTreeMeta *meta = (BTreeMeta *)meta_frame->data;
        meta->total_pages++;
        buf_mark_dirty(space_id, 0, 0);
    }

    // 2. 为了容纳新建，我们先创建一个临时的大数组
    InternalEntry temp_entries[MAX_INTERNAL_ENTRIES + 1];

    // 找到新键的插入位置并拷贝
    int pos = 0;
    // 循环的作用是：遍历内节点data，找到比key小的所有元素
    while (pos < MAX_INTERNAL_ENTRIES && page->entries[pos].key < key)
    {
        temp_entries[pos] = page->entries[pos];
        pos++;
    }
    temp_entries[pos].key = key;
    temp_entries[pos].child_page_no = child_page_no;
    for (int i = pos; i < MAX_INTERNAL_ENTRIES; i++)
    {
        temp_entries[i + 1] = page->entries[i];
    }

    // 3. 找到中间键，这个键将被“推向”父节点
    int mid_idx = (MAX_INTERNAL_ENTRIES + 1) / 2;
    *out_split_key = temp_entries[mid_idx].key;

    // 4. 将中间键之前的部分，留在老页
    page->num_entries = mid_idx;
    memcpy(page->entries, temp_entries, mid_idx * sizeof(InternalEntry));

    // 5. 将中间键之后的部分，移动到新页
    new_page->num_entries = MAX_INTERNAL_ENTRIES - mid_idx;
    // 新页的第一个孩子指针，是中间键指向的孩子
    new_page->first_child_page_no = temp_entries[mid_idx].child_page_no;
    memcpy(new_page->entries, &temp_entries[mid_idx + 1], new_page->num_entries * sizeof(InternalEntry));
    // 6. 标记两个页都为脏页
    buf_mark_dirty(space_id, page_no, 0);
    buf_mark_dirty(space_id, new_page_no, 0);

    return 0;
} */

// [最终修复] 这是内节点分裂的完整、健壮、且清理了内存的实现
// [最终修复] 这是内节点分裂的完整、健壮、且逻辑正确的实现
int internal_page_insert_or_split(uint32_t space_id, uint32_t page_no, uint32_t key, uint32_t child_page_no, uint32_t *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;
    InternalPage *page = (InternalPage *)frame->data;

    if (page->num_entries < MAX_INTERNAL_ENTRIES)
    {
        // 頁未滿，直接調用原有的插入函數即可
        return internal_page_insert(space_id, page_no, page, key, child_page_no);
    }

    // --- 頁已滿，執行穩健的分裂邏輯 ---
    printf("Internal page %u is full, splitting...\n", page_no);

    // 1. [關鍵] 創建臨时的、分離的鍵數組和指針數組
    // 指針比鍵多一個，這非常重要！
    uint32_t all_keys[MAX_INTERNAL_ENTRIES + 1];
    uint32_t all_children[MAX_INTERNAL_ENTRIES + 2];

    // 2. [關鍵] 首先將 first_child_page_no 拷貝到新數組的第一個位置
    all_children[0] = page->first_child_page_no;
    // 然後再拷貝 entries 數組中的所有鍵和指針
    for (int i = 0; i < page->num_entries; i++)
    {
        all_keys[i] = page->entries[i].key;
        all_children[i + 1] = page->entries[i].child_page_no;
    }

    // 3. 找到新鍵和新指針應該插入的位置
    int pos = 0;
    while (pos < page->num_entries && all_keys[pos] < key)
    {
        pos++;
    }

    // 4. 在臨時序列中，為新鍵和新指針騰出空間
    for (int i = page->num_entries; i > pos; i--)
    {
        all_keys[i] = all_keys[i - 1];
    }
    for (int i = page->num_entries + 1; i > pos + 1; i--)
    {
        all_children[i] = all_children[i - 1];
    }

    // 5. 插入新鍵和新指針
    all_keys[pos] = key;
    all_children[pos + 1] = child_page_no;

    // 6. 計算分裂點，並將中間鍵向上推給父節點
    int mid_idx = (MAX_INTERNAL_ENTRIES + 1) / 2;
    *out_split_key = all_keys[mid_idx];

    // 7. 分配一個新頁用於分裂
    uint32_t new_page_no;
    BufferFrame *new_frame = buf_alloc_frame(space_id, PAGE_TYPE_INTERNAL, &new_page_no);
    if (!new_frame)
        return -1;
    *out_new_page_no = new_page_no;
    InternalPage *new_page = (InternalPage *)new_frame->data;

    // 更新元數據中的總頁數
    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    if (meta_frame)
    {
        ((BTreeMeta *)meta_frame->data)->total_pages++;
        buf_mark_dirty(space_id, 0, 0);
    }

    // --- 核心邏輯：用分裂後的前半部分重寫老頁 ---
    page->num_entries = mid_idx;
    // 老頁的 first_child_page_no 維持不變, 它就是 all_children[0]
    for (int i = 0; i < mid_idx; i++)
    {
        page->entries[i].key = all_keys[i];
        page->entries[i].child_page_no = all_children[i + 1];
    }
    // 將老頁中剩餘的“垃圾”空間徹底清零，增加穩健性
    int remaining_space_old = MAX_INTERNAL_ENTRIES - mid_idx;
    if (remaining_space_old > 0)
    {
        memset(&page->entries[mid_idx], 0, remaining_space_old * sizeof(InternalEntry));
    }

    // --- 核心邏輯：用分裂後的後半部分填充新頁 ---
    new_page->num_entries = MAX_INTERNAL_ENTRIES - mid_idx;
    new_page->first_child_page_no = all_children[mid_idx + 1]; // [關鍵] 新頁的第一个指针，是分裂點之後的那個指針
    for (int i = 0; i < new_page->num_entries; i++)
    {
        new_page->entries[i].key = all_keys[mid_idx + 1 + i];
        new_page->entries[i].child_page_no = all_children[mid_idx + 2 + i];
    }
    // 將新頁中剩餘的“垃圾”空間徹底清零
    int remaining_space_new = MAX_INTERNAL_ENTRIES - new_page->num_entries;
    if (remaining_space_new > 0)
    {
        memset(&new_page->entries[new_page->num_entries], 0, remaining_space_new * sizeof(InternalEntry));
    }

    // 9. 標記髒頁
    buf_mark_dirty(space_id, page_no, 0);
    // buf_alloc_frame 已經將新頁標記為髒頁

    return 0;
}

// [重写] internal_page_get_child_index_by_page
// 在 b_tree.c 中声明为 static，因为它只在这里被 b_tree_delete_internal 使用
int internal_page_get_child_index_by_page(InternalPage *page, uint32_t child_page_no)
{
    if (page->first_child_page_no == child_page_no)
    {
        return -1;
    }
    for (int i = 0; i < page->num_entries; i++)
    {
        if (page->entries[i].child_page_no == child_page_no)
        {
            return i;
        }
    }
    return -2;
}

// [最终修复] 用一个全新的、“按键删除”的函数，替换掉旧的 internal_page_delete_entry
void remove_entry_from_parent(uint32_t space_id, uint32_t parent_page_no, uint32_t child_page_no_to_remove)
{
    BufferFrame *parent_frame = buf_get_frame(space_id, parent_page_no);
    if (!parent_frame)
        return;
    InternalPage *parent_page = (InternalPage *)parent_frame->data;

    int entry_index = -1;
    for (int i = 0; i < parent_page->num_entries; i++)
    {
        if (parent_page->entries[i].child_page_no == child_page_no_to_remove)
        {
            entry_index = i;
            break;
        }
    }

    if (entry_index != -1)
    {
        internal_page_delete_entry(space_id, parent_page_no, parent_page, entry_index);
    }
}

// [新增] 从内节点中删除一个条目
void internal_page_delete_entry(uint32_t space_id, uint32_t page_no, InternalPage *page, int entry_index)
{
    if (entry_index < 0 || entry_index >= page->num_entries)
    {
        return; // 无效索引
    }
    // 将指定索引之后的所有条目，向前移动一位
    for (int i = entry_index; i < page->num_entries - 1; i++)
    {
        page->entries[i] = page->entries[i + 1];
    }
    page->num_entries--;
    buf_mark_dirty(space_id, page_no, 0);
}