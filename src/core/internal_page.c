#include "internal_page.h"
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

    buf_mark_dirty(space_id, page_no);

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
        buf_mark_dirty(space_id, 0);
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
    buf_mark_dirty(space_id, page_no);
    buf_mark_dirty(space_id, new_page_no);

    return 0;
} */

// [最终修复] 这是内节点分裂的完整、健壮、且清理了内存的实现
int internal_page_insert_or_split(uint32_t space_id, uint32_t page_no, uint32_t key, uint32_t child_page_no, uint32_t *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;
    InternalPage *page = (InternalPage *)frame->data;

    if (page->num_entries < MAX_INTERNAL_ENTRIES)
    {
        return internal_page_insert(space_id, page_no, page, key, child_page_no);
    }

    printf("Internal page %u is full, splitting...\n", page_no);

    InternalEntry temp_entries[MAX_INTERNAL_ENTRIES + 1];
    int pos = 0;
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

    int mid_idx = (MAX_INTERNAL_ENTRIES + 1) / 2;
    *out_split_key = temp_entries[mid_idx].key;

    uint32_t new_page_no;
    BufferFrame *new_frame = buf_alloc_frame(space_id, PAGE_TYPE_INTERNAL, &new_page_no);
    if (!new_frame)
        return -1;
    *out_new_page_no = new_page_no;
    InternalPage *new_page = (InternalPage *)new_frame->data;

    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    if (meta_frame)
    {
        ((BTreeMeta *)meta_frame->data)->total_pages++;
        buf_mark_dirty(space_id, 0);
    }

    // --- 核心修复：清理并填充老页 ---
    page->num_entries = mid_idx;
    memcpy(page->entries, temp_entries, mid_idx * sizeof(InternalEntry));
    // [新增] 将老页中剩余的“垃圾”空间彻底清零
    int remaining_space_old = MAX_INTERNAL_ENTRIES - mid_idx;
    memset(&page->entries[mid_idx], 0, remaining_space_old * sizeof(InternalEntry));

    // --- 核心修复：清理并填充新页 ---
    new_page->num_entries = MAX_INTERNAL_ENTRIES - mid_idx;
    new_page->first_child_page_no = temp_entries[mid_idx].child_page_no;
    memcpy(new_page->entries, &temp_entries[mid_idx + 1], new_page->num_entries * sizeof(InternalEntry));
    // [新增] 将新页中剩余的“垃圾”空间彻底清零
    int remaining_space_new = MAX_INTERNAL_ENTRIES - new_page->num_entries;
    if (remaining_space_new > 0)
    {
        memset(&new_page->entries[new_page->num_entries], 0, remaining_space_new * sizeof(InternalEntry));
    }

    buf_mark_dirty(space_id, page_no);
    // buf_alloc_frame 已经将新页标记为脏

    return 0;
}