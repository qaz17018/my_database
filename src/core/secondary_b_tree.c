#include <stdio.h>
#include <string.h>
#include "b_tree.h" // 需要引入它来获取Meta定义
#include "buffer_pool.h"
#include "secondary_index_page.h"
#include "secondary_b_tree.h"

// 声明内部函数
static int secondary_b_tree_insert_internal(uint32_t space_id, uint32_t page_no, const SecondaryLeafEntry *entry,
                                            char *out_split_key, uint32_t *out_new_page_no);

static int secondary_b_tree_delete_internal(uint32_t space_id, uint32_t parent_page_no, uint32_t current_page_no, const char *key, uint32_t primary_key);
static void handle_secondary_underflow(uint32_t space_id, uint32_t parent_page_no, uint32_t child_page_no);
static void secondary_leaf_page_rebalance(uint32_t space_id, uint32_t parent_page_no, uint32_t left_no, uint32_t right_no);
static void secondary_leaf_page_merge(uint32_t space_id, uint32_t parent_page_no, uint32_t left_no, uint32_t right_no);
static void secondary_internal_page_delete_entry(uint32_t space_id, uint32_t page_no, SecondaryInternalPage *page, int entry_index);
static int secondary_internal_page_get_child_index_by_page(SecondaryInternalPage *page, uint32_t child_page_no);

// (这个文件的其余函数是为了辅助索引服务的，逻辑与b_tree.c类似，但使用字符串比较)

// [最终修复] 这是辅助索引内节点分裂的终极、完整、且逻辑正确的实现
static int secondary_internal_page_insert_or_split(uint32_t space_id, uint32_t page_no, const char *key, uint32_t child_page_no,
                                                   char *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;
    SecondaryInternalPage *page = (SecondaryInternalPage *)frame->data;

    // 如果未满，直接插入 (这部分逻辑是正确的)
    if (page->num_entries < MAX_SECONDARY_INTERNAL_ENTRIES)
    {
        int pos = 0;
        while (pos < page->num_entries && strcmp(page->entries[pos].key, key) < 0)
            pos++;
        for (int i = page->num_entries; i > pos; i--)
            page->entries[i] = page->entries[i - 1];
        strcpy(page->entries[pos].key, key);
        page->entries[pos].child_page_no = child_page_no;
        page->num_entries++;
        buf_mark_dirty(space_id, page_no, 0);
        return 0;
    }

    // --- 内节点分裂的正确实现 ---

    // 1. 创建临时的、能容纳所有指针和键的序列
    char all_keys[MAX_SECONDARY_INTERNAL_ENTRIES + 1][USERNAME_MAX_LEN];
    uint32_t all_children[MAX_SECONDARY_INTERNAL_ENTRIES + 2];

    // 拷贝旧页的所有指针和键，确保包含 first_child_page_no
    all_children[0] = page->first_child_page_no;
    for (int i = 0; i < page->num_entries; i++)
    {
        strcpy(all_keys[i], page->entries[i].key);
        all_children[i + 1] = page->entries[i].child_page_no;
    }

    // 找到新键的插入位置
    int pos = 0;
    while (pos < page->num_entries && strcmp(all_keys[pos], key) < 0)
        pos++;

    // 在临时序列中插入新键和新指针
    // --- [在这里进行修改：将一个错误的循环拆分为两个正确的循环] ---
    // 为新 key 腾出空间
    for (int i = page->num_entries; i > pos; i--)
    {
        strcpy(all_keys[i], all_keys[i - 1]);
    }
    // 为新 child_page_no 腾出空间 (这是独立的、正确的循环)
    for (int i = page->num_entries + 1; i > pos + 1; i--)
    {
        all_children[i] = all_children[i - 1];
    }
    strcpy(all_keys[pos], key);
    all_children[pos + 1] = child_page_no;

    // 2. 计算分裂点，并将中间键向上推
    int mid_idx = (MAX_SECONDARY_INTERNAL_ENTRIES + 1) / 2;
    strcpy(out_split_key, all_keys[mid_idx]);

    // 3. 分配新页
    uint32_t new_page_no;
    BufferFrame *new_frame = buf_alloc_frame(space_id, PAGE_TYPE_SECONDARY_INTERNAL, &new_page_no);
    if (!new_frame)
        return -1;
    *out_new_page_no = new_page_no;
    SecondaryInternalPage *new_page = (SecondaryInternalPage *)new_frame->data;
    ((BTreeMeta *)buf_get_frame(space_id, 0)->data)->total_pages++;
    buf_mark_dirty(space_id, 0, 0);

    // 4. 用分裂后的前半部分，重写老页
    page->num_entries = mid_idx;
    // first_child_page_no 不变，因为它就是 all_children[0]
    for (int i = 0; i < mid_idx; i++)
    {
        strcpy(page->entries[i].key, all_keys[i]);
        page->entries[i].child_page_no = all_children[i + 1];
    }

    // 5. 用分裂后的后半部分，填充新页
    new_page->num_entries = MAX_SECONDARY_INTERNAL_ENTRIES - mid_idx;
    new_page->first_child_page_no = all_children[mid_idx + 1];
    for (int i = 0; i < new_page->num_entries; i++)
    {
        strcpy(new_page->entries[i].key, all_keys[mid_idx + 1 + i]);
        new_page->entries[i].child_page_no = all_children[mid_idx + 2 + i];
    }

    buf_mark_dirty(space_id, page_no, 0);
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
        buf_mark_dirty(space_id, 0, 0);
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

    strcpy(out_split_key, new_page->entries[0].key);

    if (strcmp(entry->key, out_split_key) < 0)
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
        BufferFrame *root_frame = buf_alloc_frame(space_id, PAGE_TYPE_SECONDARY_LEAF, &root_page_no);
        if (!root_frame)
            return -1;

        // --- [在这里添加修改] ---
        // 关键修复：在 buf_alloc_frame 之后，重新获取 meta 指针，因为它可能已失效
        meta = get_meta(space_id);
        if (!meta)
            return -1;
        // --- [修改结束] ---

        meta->username_idx_root_page_no = root_page_no;
        meta->total_pages++;
        // 你可以继续保持下面这行的注释状态
        buf_mark_dirty(space_id, 0, 0);
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

        // --- [在这里添加修改] ---
        // 同样的，重新获取 meta 指针
        meta = get_meta(space_id);
        if (!meta)
            return -1;
        // --- [修改结束] ---

        meta->total_pages++;

        SecondaryInternalPage *new_root = (SecondaryInternalPage *)new_root_frame->data;
        new_root->first_child_page_no = old_root_page_no;
        strcpy(new_root->entries[0].key, split_key);
        new_root->entries[0].child_page_no = new_page_no;
        new_root->num_entries = 1;

        meta = (BTreeMeta *)buf_get_frame(space_id, 0)->data;
        meta->username_idx_root_page_no = new_root_page_no;
        buf_mark_dirty(space_id, 0, 0);
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

    // Case 2: 如果是内节点
    if (frame->page_type == PAGE_TYPE_SECONDARY_INTERNAL)
    {
        SecondaryInternalPage *page = (SecondaryInternalPage *)frame->data;
        uint32_t child_page_no = secondary_internal_page_get_child(page, entry->key);

        // [核心修复] 使用临时的局部变量来接收下层递归调用的分裂结果
        char child_split_key[USERNAME_MAX_LEN] = {0};
        uint32_t child_new_page_no = 0;

        // 让下层递归把结果写入这些安全的临时变量
        if (secondary_b_tree_insert_internal(space_id, child_page_no, entry, child_split_key, &child_new_page_no) != 0)
        {
            return -1;
        }

        // 检查下层是否真的发生了分裂
        if (child_split_key[0] != '\0')
        {
            // 如果下层分裂了，我们现在用这些安全、干净的临时变量作为输入，
            // 去调用当前节点的插入/分裂函数。
            // 并且，把我们自己的输出参数 out_split_key 和 out_new_page_no 传递给它，
            // 以便它能向上层报告自己的分裂情况。
            return secondary_internal_page_insert_or_split(
                space_id,
                page_no,
                child_split_key,   // <-- 使用安全的临时变量作为输入
                child_new_page_no, // <-- 使用安全的临时变量作为输入
                out_split_key,     // <-- 传递自己的输出参数
                out_new_page_no    // <-- 传递自己的输出参数
            );
        }

        // 如果下层没有分裂，万事大吉
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

static void remove_child_entry_from_parent_secondary(uint32_t space_id, uint32_t parent_page_no, uint32_t child_page_no)
{
    BufferFrame *parent_frame = buf_get_frame(space_id, parent_page_no);
    if (!parent_frame)
        return;
    SecondaryInternalPage *parent_page = (SecondaryInternalPage *)parent_frame->data;

    int entry_index = secondary_internal_page_get_child_index_by_page(parent_page, child_page_no);
    if (entry_index != -2)
    { // -2 means not found
        secondary_internal_page_delete_entry(space_id, parent_page_no, parent_page, entry_index);
    }
}

static int secondary_b_tree_delete_internal(uint32_t space_id, uint32_t parent_page_no, uint32_t current_page_no, const char *key, uint32_t primary_key)
{
    if (current_page_no == 0)
        return -1;
    BufferFrame *frame = buf_get_frame(space_id, current_page_no);
    if (!frame)
        return -1;

    if (frame->page_type == PAGE_TYPE_SECONDARY_LEAF)
    {
        return secondary_leaf_page_delete(space_id, current_page_no, (SecondaryLeafPage *)frame->data, key, primary_key);
    }

    if (frame->page_type == PAGE_TYPE_SECONDARY_INTERNAL)
    {
        SecondaryInternalPage *page = (SecondaryInternalPage *)frame->data;
        uint32_t child_page_no = secondary_internal_page_get_child(page, key);

        if (secondary_b_tree_delete_internal(space_id, current_page_no, child_page_no, key, primary_key) != 0)
        {
            return -1;
        }

        BufferFrame *child_frame = buf_get_frame(space_id, child_page_no);
        if (!child_frame)
            return -1;

        int underflow = 0;
        if (child_frame->page_type == PAGE_TYPE_SECONDARY_LEAF)
        {
            if (((SecondaryLeafPage *)child_frame->data)->num_entries < MAX_SECONDARY_LEAF_ENTRIES / 2)
                underflow = 1;
        }
        else
        {
            if (((SecondaryInternalPage *)child_frame->data)->num_entries < MAX_SECONDARY_INTERNAL_ENTRIES / 2)
                underflow = 1;
        }

        if (underflow)
        {
            handle_secondary_underflow(space_id, current_page_no, child_page_no);
        }
        return 0;
    }
    return -1;
}

int secondary_b_tree_delete(uint32_t space_id, const char *key, uint32_t primary_key)
{
    BTreeMeta *meta = get_meta(space_id);
    if (!meta || meta->username_idx_root_page_no == 0)
        return -1;

    if (secondary_b_tree_delete_internal(space_id, 0, meta->username_idx_root_page_no, key, primary_key) != 0)
    {
        return -1;
    }

    // 检查根节点收缩
    meta = get_meta(space_id);
    uint32_t root_page_no = meta->username_idx_root_page_no;
    BufferFrame *root_frame = buf_get_frame(space_id, root_page_no);
    if (root_frame && root_frame->page_type == PAGE_TYPE_SECONDARY_INTERNAL && ((SecondaryInternalPage *)root_frame->data)->num_entries == 0)
    {
        meta->username_idx_root_page_no = ((SecondaryInternalPage *)root_frame->data)->first_child_page_no;
        buf_mark_dirty(space_id, 0, 0);
    }
    return 0;
}

static void secondary_internal_page_delete_entry(uint32_t space_id, uint32_t page_no, SecondaryInternalPage *page, int entry_index)
{
    if (entry_index < 0 || entry_index >= page->num_entries)
        return;
    for (int i = entry_index; i < page->num_entries - 1; i++)
    {
        page->entries[i] = page->entries[i + 1];
    }
    page->num_entries--;
    buf_mark_dirty(space_id, page_no, 0);
}

static void secondary_leaf_page_merge(uint32_t space_id, uint32_t parent_page_no, uint32_t left_no, uint32_t right_no)
{
    BufferFrame *left_frame = buf_get_frame(space_id, left_no);
    BufferFrame *right_frame = buf_get_frame(space_id, right_no);
    if (!left_frame || !right_frame)
        return;
    SecondaryLeafPage *left_page = (SecondaryLeafPage *)left_frame->data;
    SecondaryLeafPage *right_page = (SecondaryLeafPage *)right_frame->data;

    memcpy(&left_page->entries[left_page->num_entries], right_page->entries, right_page->num_entries * sizeof(SecondaryLeafEntry));
    left_page->num_entries += right_page->num_entries;
    left_page->next_leaf = right_page->next_leaf;

    buf_mark_dirty(space_id, left_no, 0);
    remove_child_entry_from_parent_secondary(space_id, parent_page_no, right_no);
}

static void secondary_leaf_page_rebalance(uint32_t space_id, uint32_t parent_page_no, uint32_t left_no, uint32_t right_no)
{
    BufferFrame *parent_frame = buf_get_frame(space_id, parent_page_no);
    BufferFrame *left_frame = buf_get_frame(space_id, left_no);
    BufferFrame *right_frame = buf_get_frame(space_id, right_no);
    if (!parent_frame || !left_frame || !right_frame)
        return;
    SecondaryInternalPage *parent_page = (SecondaryInternalPage *)parent_frame->data;
    SecondaryLeafPage *left_page = (SecondaryLeafPage *)left_frame->data;
    SecondaryLeafPage *right_page = (SecondaryLeafPage *)right_frame->data;

    int separator_index = secondary_internal_page_get_child_index_by_page(parent_page, right_no);
    if (separator_index == -2)
        return;

    if (left_page->num_entries < right_page->num_entries)
    {
        SecondaryLeafEntry entry_to_move = right_page->entries[0];
        secondary_leaf_page_insert(space_id, left_no, left_page, &entry_to_move);
        secondary_leaf_page_delete(space_id, right_no, right_page, entry_to_move.key, entry_to_move.primary_key);
        strcpy(parent_page->entries[separator_index].key, right_page->entries[0].key);
    }
    else
    {
        SecondaryLeafEntry entry_to_move = left_page->entries[left_page->num_entries - 1];
        secondary_leaf_page_delete(space_id, left_no, left_page, entry_to_move.key, entry_to_move.primary_key);
        secondary_leaf_page_insert(space_id, right_no, right_page, &entry_to_move);
        strcpy(parent_page->entries[separator_index].key, right_page->entries[0].key);
    }
    buf_mark_dirty(space_id, parent_page_no, 0);
}

static void secondary_internal_page_merge(uint32_t space_id, uint32_t parent_page_no, uint32_t left_no, uint32_t right_no)
{
    BufferFrame *parent_frame = buf_get_frame(space_id, parent_page_no);
    BufferFrame *left_frame = buf_get_frame(space_id, left_no);
    BufferFrame *right_frame = buf_get_frame(space_id, right_no);
    if (!parent_frame || !left_frame || !right_frame)
        return;
    SecondaryInternalPage *parent_page = (SecondaryInternalPage *)parent_frame->data;
    SecondaryInternalPage *left_page = (SecondaryInternalPage *)left_frame->data;
    SecondaryInternalPage *right_page = (SecondaryInternalPage *)right_frame->data;

    int separator_index = secondary_internal_page_get_child_index_by_page(parent_page, right_no);
    if (separator_index < 0)
        return;

    strcpy(left_page->entries[left_page->num_entries].key, parent_page->entries[separator_index].key);
    left_page->entries[left_page->num_entries].child_page_no = right_page->first_child_page_no;
    left_page->num_entries++;

    memcpy(&left_page->entries[left_page->num_entries], right_page->entries, right_page->num_entries * sizeof(SecondaryInternalEntry));
    left_page->num_entries += right_page->num_entries;

    buf_mark_dirty(space_id, left_no, 0);
    remove_child_entry_from_parent_secondary(space_id, parent_page_no, right_no);
}

static void secondary_internal_page_rebalance(uint32_t space_id, uint32_t parent_page_no, uint32_t left_no, uint32_t right_no)
{
    BufferFrame *parent_frame = buf_get_frame(space_id, parent_page_no);
    BufferFrame *left_frame = buf_get_frame(space_id, left_no);
    BufferFrame *right_frame = buf_get_frame(space_id, right_no);
    if (!parent_frame || !left_frame || !right_frame)
        return;
    SecondaryInternalPage *parent_page = (SecondaryInternalPage *)parent_frame->data;
    SecondaryInternalPage *left_page = (SecondaryInternalPage *)left_frame->data;
    SecondaryInternalPage *right_page = (SecondaryInternalPage *)right_frame->data;

    int separator_index = secondary_internal_page_get_child_index_by_page(parent_page, right_no);
    if (separator_index < 0)
        return;

    if (left_page->num_entries < right_page->num_entries)
    {
        strcpy(left_page->entries[left_page->num_entries].key, parent_page->entries[separator_index].key);
        left_page->entries[left_page->num_entries].child_page_no = right_page->first_child_page_no;
        left_page->num_entries++;

        strcpy(parent_page->entries[separator_index].key, right_page->entries[0].key);
        right_page->first_child_page_no = right_page->entries[0].child_page_no;
        secondary_internal_page_delete_entry(space_id, right_no, right_page, 0);
    }
    else
    {
        secondary_internal_page_insert(space_id, right_no, right_page, parent_page->entries[separator_index].key, right_page->first_child_page_no);
        right_page->first_child_page_no = left_page->entries[left_page->num_entries - 1].child_page_no;
        strcpy(parent_page->entries[separator_index].key, left_page->entries[left_page->num_entries - 1].key);
        secondary_internal_page_delete_entry(space_id, left_no, left_page, left_page->num_entries - 1);
    }
    buf_mark_dirty(space_id, parent_page_no, 0);
}

// [新增] 辅助索引欠载处理调度
static void handle_secondary_underflow(uint32_t space_id, uint32_t parent_page_no, uint32_t child_page_no)
{
    if (parent_page_no == 0)
        return;

    BufferFrame *parent_frame = buf_get_frame(space_id, parent_page_no);
    if (!parent_frame)
        return;
    SecondaryInternalPage *parent_page = (SecondaryInternalPage *)parent_frame->data;

    int child_index = secondary_internal_page_get_child_index_by_page(parent_page, child_page_no);
    uint32_t left_s_no = 0, right_s_no = 0;

    if (child_index == -1)
    {
        right_s_no = parent_page->num_entries > 0 ? parent_page->entries[0].child_page_no : 0;
    }
    else
    {
        left_s_no = (child_index == 0) ? parent_page->first_child_page_no : parent_page->entries[child_index - 1].child_page_no;
        right_s_no = (child_index < parent_page->num_entries - 1) ? parent_page->entries[child_index + 1].child_page_no : 0;
    }

    BufferFrame *child_frame = buf_get_frame(space_id, child_page_no);
    if (child_frame->page_type == PAGE_TYPE_SECONDARY_LEAF)
    {
        if (left_s_no != 0)
        {
            BufferFrame *left_s_frame = buf_get_frame(space_id, left_s_no);
            if ((((SecondaryLeafPage *)left_s_frame->data)->num_entries + ((SecondaryLeafPage *)child_frame->data)->num_entries) <= MAX_SECONDARY_LEAF_ENTRIES)
            {
                secondary_leaf_page_merge(space_id, parent_page_no, left_s_no, child_page_no);
            }
            else
            {
                secondary_leaf_page_rebalance(space_id, parent_page_no, left_s_no, child_page_no);
            }
        }
        else if (right_s_no != 0)
        {
            BufferFrame *right_s_frame = buf_get_frame(space_id, right_s_no);
            if ((((SecondaryLeafPage *)child_frame->data)->num_entries + ((SecondaryLeafPage *)right_s_frame->data)->num_entries) <= MAX_SECONDARY_LEAF_ENTRIES)
            {
                secondary_leaf_page_merge(space_id, parent_page_no, child_page_no, right_s_no);
            }
            else
            {
                secondary_leaf_page_rebalance(space_id, parent_page_no, child_page_no, right_s_no);
            }
        }
    }
    else
    { // Internal node
        if (left_s_no != 0)
        {
            BufferFrame *left_s_frame = buf_get_frame(space_id, left_s_no);
            if ((((SecondaryInternalPage *)left_s_frame->data)->num_entries + ((SecondaryInternalPage *)child_frame->data)->num_entries) < MAX_SECONDARY_INTERNAL_ENTRIES)
            {
                secondary_internal_page_merge(space_id, parent_page_no, left_s_no, child_page_no);
            }
            else
            {
                secondary_internal_page_rebalance(space_id, parent_page_no, left_s_no, child_page_no);
            }
        }
        else if (right_s_no != 0)
        {
            BufferFrame *right_s_frame = buf_get_frame(space_id, right_s_no);
            if ((((SecondaryInternalPage *)child_frame->data)->num_entries + ((SecondaryInternalPage *)right_s_frame->data)->num_entries) < MAX_SECONDARY_INTERNAL_ENTRIES)
            {
                secondary_internal_page_merge(space_id, parent_page_no, child_page_no, right_s_no);
            }
            else
            {
                secondary_internal_page_rebalance(space_id, parent_page_no, child_page_no, right_s_no);
            }
        }
    }
}

/**
 * @brief [新增] 辅助函数，通过子页号查找其在父节点中的索引
 */
static int secondary_internal_page_get_child_index_by_page(SecondaryInternalPage *page, uint32_t child_page_no)
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
    return -2; // Not found
}

static int secondary_internal_page_insert(uint32_t space_id, uint32_t page_no, SecondaryInternalPage *page, const char *key, uint32_t child_page_no)
{
    if (page->num_entries >= MAX_SECONDARY_INTERNAL_ENTRIES)
    {
        return -1; // Should not happen if called correctly
    }

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

    buf_mark_dirty(space_id, page_no, 0);
    return 0;
}