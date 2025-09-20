#include "b_tree.h"
#include "buffer_pool.h"
#include "leaf_page.h"
#include "internal_page.h"
#include "secondary_b_tree.h"
#include <stdio.h>
#include <string.h>
#include "sql_utils.h"

static int b_tree_insert_internal(uint32_t space_id, uint32_t page_no, const Row *row, uint32_t *out_split_key, uint32_t *out_new_page_no);
static uint32_t b_tree_find_leaf_page_internal(uint32_t space_id, uint32_t page_no, uint32_t id);
static uint32_t b_tree_find_leaf_page(uint32_t space_id, uint32_t id);
static void handle_leaf_underflow(uint32_t space_id, uint32_t parent_page_no, InternalPage *parent_page, uint32_t child_page_no, int child_index);
static void handle_internal_underflow(uint32_t space_id, uint32_t parent_page_no, InternalPage *parent_page, uint32_t child_page_no, int child_index);
static void leaf_page_redistribute(uint32_t space_id, uint32_t parent_page_no, InternalPage *parent_page, int underflow_child_index,
                                   uint32_t underflow_page_no, LeafPage *underflow_page,
                                   uint32_t donor_page_no, LeafPage *donor_page);
static void leaf_page_merge(uint32_t space_id, uint32_t parent_page_no,
                            uint32_t left_page_no, uint32_t right_page_no);
static void internal_page_redistribute(uint32_t space_id, uint32_t parent_page_no, InternalPage *parent_page, int underflow_child_index,
                                       uint32_t underflow_page_no, InternalPage *underflow_page,
                                       uint32_t donor_page_no, InternalPage *donor_page);
static void internal_page_merge(uint32_t space_id, uint32_t parent_page_no,
                                uint32_t left_page_no, uint32_t right_page_no);

BTreeMeta *get_meta(uint32_t space_id)
{
    BufferFrame *frame = buf_get_frame(space_id, 0);
    return frame ? (BTreeMeta *)frame->data : NULL;
}

int secondary_b_tree_insert(uint32_t space_id, const SecondaryLeafEntry *entry);

// [最终修复] 这是B+树创世的终极、完整、且逻辑正确的实现
int b_tree_create(uint32_t space_id)
{
    // --- 步骤1: 创世之初，先创建户籍管理员（元数据页） ---
    // 我们分配的第一个页，必须是0号页，专门用于存储元数据。
    uint32_t meta_page_no;
    BufferFrame *meta_frame = buf_alloc_frame(space_id, PAGE_TYPE_META, &meta_page_no);
    // 做一个严格的断言，确保创世的第一个页就是0号页
    if (!meta_frame || meta_page_no != 0)
    {
        fprintf(stderr, "Fatal Error: The first page allocated was not page 0. Creation failed.\n");
        return -1;
    }
    BTreeMeta *meta = (BTreeMeta *)meta_frame->data;

    // --- 步骤2: 有了管理员，再创建第一个居民（根节点） ---
    // 我们分配的第二个页，将是1号页，它将作为B+树的第一个根，其类型为叶子。
    uint32_t root_page_no;
    BufferFrame *root_frame = buf_alloc_frame(space_id, PAGE_TYPE_LEAF, &root_page_no);
    if (!root_frame || root_page_no != 1)
    {
        fprintf(stderr, "Fatal Error: The second page allocated was not page 1. Creation failed.\n");
        return -1;
    }

    // --- 步骤3: 管理员记录下第一个居民的住址 ---
    // 初始化元数据，让它指向我们刚刚创建的根节点
    meta->root_page_no = root_page_no;
    meta->username_idx_root_page_no = 0;
    meta->total_pages = 2; // 现在我们有了0号（meta）和1号（root）两个页

    // 4. 将元数据的修改登记在案（标记为脏页），以便写回磁盘
    // buf_mark_dirty(space_id, meta_page_no);

    printf("B+Tree created for space %u. Meta Page: %u, Root Page (Leaf): %u\n",
           space_id, meta_page_no, root_page_no);

    return 0;
}

static int b_tree_insert_internal(uint32_t space_id, uint32_t page_no, const Row *row,
                                  uint32_t *out_split_key, uint32_t *out_new_page_no)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
    {
        fprintf(stderr, "Error: Could not get frame for page %u.\n", page_no);
        return -1;
    }

    // [核心修复] 我们不再直接读取页头，而是完全相信Buffer Pool中记录的页面类型
    // 这也是我们上次重构接口的最终目的
    if (frame->page_type == PAGE_TYPE_LEAF)
    {
        LeafPage *leaf = (LeafPage *)frame->data;
        // 如果是叶子节点，直接调用叶子页的插入或分裂函数
        return leaf_page_insert_or_split(space_id, page_no, row, out_new_page_no, out_split_key);
    }

    // 如果是内节点
    if (frame->page_type == PAGE_TYPE_INTERNAL)
    {
        InternalPage *internal = (InternalPage *)frame->data;
        uint32_t child_page_no = internal_page_get_child(internal, row->id);

        // 递归进入子节点
        b_tree_insert_internal(space_id, child_page_no, row, out_split_key, out_new_page_no);

        // 如果子节点没有分裂，直接返回
        if (*out_split_key == 0)
        {
            return 0;
        }

        // 子节点发生了分裂，需要在当前内节点中插入新键
        uint32_t new_key = *out_split_key;
        uint32_t new_child_page = *out_new_page_no;

        // 重置分裂信号
        *out_split_key = 0;
        *out_new_page_no = 0;

        // 检查当前内节点是否已满
        frame = buf_get_frame(space_id, page_no);
        if (!frame)
            return -1;

        // [核心修改] 调用我们新的分裂函数
        // 它会自动处理插入或分裂，并将需要推向上一层的键通过 out_split_key 返回
        return internal_page_insert_or_split(space_id, page_no, new_key, new_child_page, out_split_key, out_new_page_no);
        /* if (internal->num_entries < MAX_INTERNAL_ENTRIES)
        {
            return internal_page_insert(space_id, page_no, internal, new_key, new_child_page);
        }
        else
        {
            // [TODO] 内节点也满了，需要分裂内节点
            printf("PANIC: Internal page %u is full, needs splitting! (Not implemented yet)\n", page_no);
            return -1;
        } */
    }

    fprintf(stderr, "Error: Unknown page type %d for page %u\n", frame->page_type, page_no);
    return -1;
}

int b_tree_insert(uint32_t space_id, const Row *row)
{
    BTreeMeta *meta = get_meta(space_id);
    if (!meta)
    {
        // 如果元数据不存在，意味着树还没创建
        printf("B-Tree not found for space %u. Creating now...\n", space_id);
        b_tree_create(space_id);
        meta = get_meta(space_id); // 重新获取
        if (!meta)
            return -1;
    }
    uint32_t root_page_no = meta->root_page_no;

    uint32_t split_key = 0;
    uint32_t new_page_no = 0;

    // [核心修复] 我们现在检查下属的返回值
    if (b_tree_insert_internal(space_id, root_page_no, row, &split_key, &new_page_no) != 0)
    {
        fprintf(stderr, "Error propagation: b_tree_insert_internal failed.\n");
        return -1; // 如果下属失败了，经理也报告失败
    }

    // 检查根节点是否发生了分裂
    if (split_key > 0)
    {
        printf("Root page %u has split! Creating a new root.\n", root_page_no);
        // 原来的根分裂了，需要创建一个新的内节点作为根
        uint32_t new_root_page_no;
        BufferFrame *new_root_frame = buf_alloc_frame(space_id, PAGE_TYPE_INTERNAL, &new_root_page_no);
        if (!new_root_frame)
            return -1;
        InternalPage *new_root = (InternalPage *)new_root_frame->data;

        // --- [在这里添加修改] ---
        // 关键修复：在 buf_alloc_frame 之后，重新获取 meta 指针
        meta = get_meta(space_id);
        if (!meta)
            return -1;
        // --- [修改结束] ---

        meta->total_pages++;

        // ...
        meta->root_page_no = new_root_page_no;
        // 你可以继续保持下面这行的注释状态
        // buf_mark_dirty(space_id, 0, 0);

        // 新根节点指向原来的老根（现在是左孩子）和分裂出的新页（右孩子）
        new_root->first_child_page_no = root_page_no;
        new_root->entries[0].key = split_key;
        new_root->entries[0].child_page_no = new_page_no;
        new_root->num_entries = 1;

        // 更新meta信息，指向新的根
        meta->root_page_no = new_root_page_no;
        // buf_mark_dirty(space_id, 0);
        printf("New root is an INTERNAL page: %u. Tree height increased.\n", new_root_page_no);
    }
    return 0;
}

// [核心实现] 表级别地插入函数
int table_insert_row(uint32_t space_id, const Row *row)
{
    // 1. 首先，将完整数据行插入到主B+树（聚簇索引）中
    if (b_tree_insert(space_id, row) != 0)
    {
        fprintf(stderr, "Error: Failed to insert into primary B-Tree.\n");
        return -1; // 如果主索引插入失败，直接返回
    }

    // 2. 主B+树插入成功后，再构造辅助索引的条目
    SecondaryLeafEntry secondary_entry;
    strncpy(secondary_entry.key, row->username, USERNAME_MAX_LEN);
    secondary_entry.primary_key = row->id;

    // 3. 将辅助索引条目插入到辅助B+树中
    if (secondary_b_tree_insert(space_id, &secondary_entry) != 0)
    {
        fprintf(stderr, "Error: Failed to insert into secondary index for username %s.\n", row->username);
        // 在真实的数据库中，这里需要回滚第一步的操作，以保证数据一致性
        return -1;
    }

    printf("Successfully inserted row (id=%u, username=%s) into both primary and secondary indexes.\n", row->id, row->username);
    return 0;
}

static Row *b_tree_search_internal(uint32_t space_id, uint32_t page_no, uint32_t id);

// 顶层的主B+树查找函数
Row *b_tree_search(uint32_t space_id, uint32_t id)
{
    BTreeMeta *meta = get_meta(space_id);
    if (!meta)
        return NULL;

    return b_tree_search_internal(space_id, meta->root_page_no, id);
}

// 内部递归查找函数
static Row *b_tree_search_internal(uint32_t space_id, uint32_t page_no, uint32_t id)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return NULL;

    if (frame->page_type == PAGE_TYPE_LEAF)
    {
        // 在叶子节点中查找
        // leaf_page.c 中还没有一个顶层的查找函数，我们先直接在这里实现
        LeafPage *leaf = (LeafPage *)frame->data;
        for (int i = 0; i < leaf->num_records; i++)
        {
            if (leaf->records[i].id == id)
            {
                // 注意：这里返回的指针指向Buffer Pool，是临时的
                // 安全的做法是拷贝一份数据出来
                static Row found_row;
                found_row = leaf->records[i];
                return &found_row;
            }
        }
        BufferFrame *frame1 = buf_get_frame(space_id, page_no);
        return NULL;
    }

    if (frame->page_type == PAGE_TYPE_INTERNAL)
    {
        uint32_t child_page_no = internal_page_get_child((InternalPage *)frame->data, id);
        return b_tree_search_internal(space_id, child_page_no, id);
    }

    return NULL;
}

// 最后，实现我们之前在头文件中定义的 table_search 函数
uint32_t table_search_pk_by_username(uint32_t space_id, const char *username)
{
    return secondary_b_tree_search(space_id, username);
}

// [新增] 查找包含指定id的叶子页的递归辅助函数
static uint32_t b_tree_find_leaf_page_internal(uint32_t space_id, uint32_t page_no, uint32_t id)
{
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return 0; // 0 代表未找到或错误

    if (frame->page_type == PAGE_TYPE_LEAF)
    {
        // 已经到达叶子层，返回当前页号
        return page_no;
    }

    if (frame->page_type == PAGE_TYPE_INTERNAL)
    {
        // 在内节点中，找到下一个要访问的子节点，并递归深入
        InternalPage *internal = (InternalPage *)frame->data;
        uint32_t child_page_no = internal_page_get_child(internal, id);
        return b_tree_find_leaf_page_internal(space_id, child_page_no, id);
    }

    return 0; // 未知页面类型
}

// [新增] 查找包含指定id的叶子页的顶层入口函数
static uint32_t b_tree_find_leaf_page(uint32_t space_id, uint32_t id)
{
    BTreeMeta *meta = get_meta(space_id);
    if (!meta || meta->root_page_no == 0)
        return 0;
    return b_tree_find_leaf_page_internal(space_id, meta->root_page_no, id);
}

// [新增] 重新分配叶子节点记录的实现
static void leaf_page_redistribute(uint32_t space_id, uint32_t parent_page_no, InternalPage *parent_page, int underflow_child_index,
                                   uint32_t underflow_page_no, LeafPage *underflow_page,
                                   uint32_t donor_page_no, LeafPage *donor_page)
{
    // 如果 donor_page 是右兄弟
    if (underflow_page->next_leaf == donor_page_no)
    {
        // 1. 从右兄弟“借”走第一个记录
        Row row_to_move = donor_page->records[0];

        // 2. 将该记录追加到欠载节点的末尾
        underflow_page->records[underflow_page->num_records] = row_to_move;
        underflow_page->num_records++;

        // 3. 更新右兄弟的内容：所有记录左移一位
        for (int i = 0; i < donor_page->num_records - 1; i++)
        {
            donor_page->records[i] = donor_page->records[i + 1];
        }
        donor_page->num_records--;

        // 4. [关键] 更新父节点中，指向右兄弟的那个“路标”键
        // 新的路标键，就是右兄弟移动后的新“最小键”
        parent_page->entries[underflow_child_index].key = donor_page->records[0].id;
    }
    // 如果 donor_page 是左兄弟
    else
    {
        // 1. 从左兄弟“借”走最后一个记录
        Row row_to_move = donor_page->records[donor_page->num_records - 1];
        donor_page->num_records--;

        // 2. 将欠载节点的所有记录右移一位，为新记录腾出空间
        for (int i = underflow_page->num_records; i > 0; i--)
        {
            underflow_page->records[i] = underflow_page->records[i - 1];
        }

        // 3. 将借来的记录插入到欠载节点的开头
        underflow_page->records[0] = row_to_move;
        underflow_page->num_records++;

        // 4. [关键] 更新父节点中，指向欠载节点(它现在是右边那个)的那个“路标”键
        // 新的路标键，就是欠载节点收到的新“最小键”
        parent_page->entries[underflow_child_index].key = underflow_page->records[0].id;
    }

    // 标记所有被修改的页为脏页
    // buf_mark_dirty(space_id, parent_page_no);
    // buf_mark_dirty(space_id, underflow_page_no);
    // buf_mark_dirty(space_id, donor_page_no);
}

// [新增] 合并两个叶子节点的实现
static void leaf_page_merge(uint32_t space_id, uint32_t parent_page_no,
                            uint32_t left_page_no, uint32_t right_page_no)
{
    BufferFrame *left_frame = buf_get_frame(space_id, left_page_no);
    BufferFrame *right_frame = buf_get_frame(space_id, right_page_no);
    if (!left_frame || !right_frame)
        return;
    LeafPage *left_page = (LeafPage *)left_frame->data;
    LeafPage *right_page = (LeafPage *)right_frame->data;

    memcpy(&left_page->records[left_page->num_records], right_page->records, right_page->num_records * sizeof(Row));
    left_page->num_records += right_page->num_records;
    left_page->next_leaf = right_page->next_leaf;

    // buf_mark_dirty(space_id, left_page_no);

    // [核心] 调用新的、按值（子页号）删除的函数
    remove_entry_from_parent(space_id, parent_page_no, right_page_no);
}

// --- 叶子节点欠载处理 ---
// --- 叶子节点欠载处理 ---
static void handle_leaf_underflow(uint32_t space_id, uint32_t parent_page_no, InternalPage *parent_page, uint32_t child_page_no, int child_index)
{
    BufferFrame *child_frame = buf_get_frame(space_id, child_page_no);
    LeafPage *child_page = (LeafPage *)child_frame->data;

    // [重写] 更健壮的兄弟查找逻辑
    uint32_t left_sibling_no = 0, right_sibling_no = 0;
    int separator_index_for_left = -1, separator_index_for_right = -1;

    if (child_index == -1)
    { // 欠载节点是最左侧的孩子
        right_sibling_no = parent_page->entries[0].child_page_no;
        separator_index_for_right = 0;
    }
    else
    {
        left_sibling_no = (child_index == 0) ? parent_page->first_child_page_no : parent_page->entries[child_index - 1].child_page_no;
        separator_index_for_left = child_index;
        if (child_index < parent_page->num_entries - 1)
        {
            right_sibling_no = parent_page->entries[child_index + 1].child_page_no;
            separator_index_for_right = child_index + 1;
        }
    }

    // 优先从右兄弟借
    if (right_sibling_no != 0)
    {
        BufferFrame *right_sibling_frame = buf_get_frame(space_id, right_sibling_no);
        LeafPage *right_sibling = (LeafPage *)right_sibling_frame->data;
        if (right_sibling->num_records > MAX_LEAF_RECORDS / 2)
        {
            leaf_page_redistribute(space_id, parent_page_no, parent_page, separator_index_for_right, child_page_no, child_page, right_sibling_no, right_sibling);
            return;
        }
    }
    // 其次从左兄弟借
    if (left_sibling_no != 0)
    {
        BufferFrame *left_sibling_frame = buf_get_frame(space_id, left_sibling_no);
        LeafPage *left_sibling = (LeafPage *)left_sibling_frame->data;
        if (left_sibling->num_records > MAX_LEAF_RECORDS / 2)
        {
            leaf_page_redistribute(space_id, parent_page_no, parent_page, separator_index_for_left, child_page_no, child_page, left_sibling_no, left_sibling);
            return;
        }
    }

    // 只能合并
    if (right_sibling_no != 0)
    {
        BufferFrame *right_sibling_frame = buf_get_frame(space_id, right_sibling_no);
        leaf_page_merge(space_id, parent_page_no, child_page_no, right_sibling_no);
    }
    else
    {
        BufferFrame *left_sibling_frame = buf_get_frame(space_id, left_sibling_no);
        leaf_page_merge(space_id, parent_page_no, left_sibling_no, child_page_no);
    }
}

// [新增] 重新分配内节点记录的实现 (这是整个B+树算法中最复杂的部分)
static void internal_page_redistribute(uint32_t space_id, uint32_t parent_page_no, InternalPage *parent_page, int underflow_child_index,
                                       uint32_t underflow_page_no, InternalPage *underflow_page,
                                       uint32_t donor_page_no, InternalPage *donor_page)
{
    // 如果 donor_page 是右兄弟
    if (underflow_page->first_child_page_no < donor_page->first_child_page_no)
    { // 简化的左右判断
        // 1. 将父节点中分隔二者的“路标”键，拉下来到欠载节点的末尾
        underflow_page->entries[underflow_page->num_entries].key = parent_page->entries[underflow_child_index].key;
        underflow_page->entries[underflow_page->num_entries].child_page_no = donor_page->first_child_page_no;
        underflow_page->num_entries++;

        // 2. 将右兄弟的第一个“路标”键，推上去替换父节点中被拉下来的键
        parent_page->entries[underflow_child_index].key = donor_page->entries[0].key;

        // 3. 更新右兄弟的内容
        donor_page->first_child_page_no = donor_page->entries[0].child_page_no;
        for (int i = 0; i < donor_page->num_entries - 1; i++)
        {
            donor_page->entries[i] = donor_page->entries[i + 1];
        }
        donor_page->num_entries--;
    }
    // 如果 donor_page 是左兄弟
    else
    {
        // 1. 将欠载节点的所有条目和指针右移一位
        for (int i = underflow_page->num_entries; i > 0; i--)
        {
            underflow_page->entries[i] = underflow_page->entries[i - 1];
        }
        underflow_page->entries[0].child_page_no = underflow_page->first_child_page_no;

        // 2. 将父节点中分隔二者的“路标”键，拉下来到欠载节点的开头
        underflow_page->entries[0].key = parent_page->entries[underflow_child_index].key;
        underflow_page->first_child_page_no = donor_page->entries[donor_page->num_entries - 1].child_page_no;
        underflow_page->num_entries++;

        // 3. 将左兄弟的最后一个“路标”键，推上去替换父节点中被拉下来的键
        parent_page->entries[underflow_child_index].key = donor_page->entries[donor_page->num_entries - 1].key;
        donor_page->num_entries--;
    }

    // buf_mark_dirty(space_id, parent_page_no);
    // buf_mark_dirty(space_id, underflow_page_no);
    // buf_mark_dirty(space_id, donor_page_no);
}

// [新增] 合并两个内节点的实现
static void internal_page_merge(uint32_t space_id, uint32_t parent_page_no,
                                uint32_t left_page_no, uint32_t right_page_no)
{
    // [指针安全] 在操作前，获取所有需要的、最新的指针
    BufferFrame *parent_frame = buf_get_frame(space_id, parent_page_no);
    BufferFrame *left_frame = buf_get_frame(space_id, left_page_no);
    BufferFrame *right_frame = buf_get_frame(space_id, right_page_no);
    if (!parent_frame || !left_frame || !right_frame)
        return;

    InternalPage *parent_page = (InternalPage *)parent_frame->data;
    InternalPage *left_page = (InternalPage *)left_frame->data;
    InternalPage *right_page = (InternalPage *)right_frame->data;

    // 找到分隔键的索引
    int separator_key_index = internal_page_get_child_index_by_page(parent_page, right_page_no);
    if (separator_key_index == -2)
        return; // 安全检查

    // 从父节点拉下分隔键
    left_page->entries[left_page->num_entries].key = parent_page->entries[separator_key_index].key;
    left_page->entries[left_page->num_entries].child_page_no = right_page->first_child_page_no;
    left_page->num_entries++;

    memcpy(&left_page->entries[left_page->num_entries], right_page->entries, right_page->num_entries * sizeof(InternalEntry));
    left_page->num_entries += right_page->num_entries;

    // buf_mark_dirty(space_id, left_page_no);

    // [核心] 调用新的、按值（子页号）删除的函数
    remove_entry_from_parent(space_id, parent_page_no, right_page_no);
}

static void handle_internal_underflow(uint32_t space_id, uint32_t parent_page_no, InternalPage *parent_page, uint32_t child_page_no, int child_index)
{
    // 逻辑与 handle_leaf_underflow 完全平行
    uint32_t left_sibling_no = 0, right_sibling_no = 0;
    int separator_index_for_left = -1, separator_index_for_right = -1;

    if (child_index == -1)
    {
        right_sibling_no = parent_page->entries[0].child_page_no;
        separator_index_for_right = 0;
    }
    else
    {
        left_sibling_no = (child_index == 0) ? parent_page->first_child_page_no : parent_page->entries[child_index - 1].child_page_no;
        separator_index_for_left = child_index;
        if (child_index < parent_page->num_entries - 1)
        {
            right_sibling_no = parent_page->entries[child_index + 1].child_page_no;
            separator_index_for_right = child_index + 1;
        }
    }

    BufferFrame *child_frame = buf_get_frame(space_id, child_page_no);
    InternalPage *child_page = (InternalPage *)child_frame->data;

    if (right_sibling_no != 0)
    {
        BufferFrame *right_sibling_frame = buf_get_frame(space_id, right_sibling_no);
        InternalPage *right_sibling = (InternalPage *)right_sibling_frame->data;
        if (right_sibling->num_entries > MAX_INTERNAL_ENTRIES / 2)
        {
            internal_page_redistribute(space_id, parent_page_no, parent_page, separator_index_for_right, child_page_no, child_page, right_sibling_no, right_sibling);
            return;
        }
    }
    if (left_sibling_no != 0)
    {
        BufferFrame *left_sibling_frame = buf_get_frame(space_id, left_sibling_no);
        InternalPage *left_sibling = (InternalPage *)left_sibling_frame->data;
        if (left_sibling->num_entries > MAX_INTERNAL_ENTRIES / 2)
        {
            internal_page_redistribute(space_id, parent_page_no, parent_page, separator_index_for_left, child_page_no, child_page, left_sibling_no, left_sibling);
            return;
        }
    }

    if (right_sibling_no != 0)
    {
        BufferFrame *right_sibling_frame = buf_get_frame(space_id, right_sibling_no);
        internal_page_merge(space_id, parent_page_no, child_page_no, right_sibling_no);
    }
    else
    {
        internal_page_merge(space_id, parent_page_no, left_sibling_no, child_page_no);
    }
}

// [重写] b_tree_delete_internal，确保指针安全
int b_tree_delete_internal(uint32_t space_id, uint32_t page_no, uint32_t id)
{
    if (page_no == 0)
        return -1;
    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return -1;

    if (frame->page_type == PAGE_TYPE_LEAF)
    {
        return leaf_page_delete(space_id, page_no, (LeafPage *)frame->data, id);
    }

    if (frame->page_type == PAGE_TYPE_INTERNAL)
    {
        InternalPage *page_snapshot = (InternalPage *)malloc(PAGE_DATA_SIZE);
        memcpy(page_snapshot, frame->data, PAGE_DATA_SIZE);

        uint32_t child_page_no = internal_page_get_child(page_snapshot, id);

        if (b_tree_delete_internal(space_id, child_page_no, id) != 0)
        {
            free(page_snapshot);
            return -1;
        }

        frame = buf_get_frame(space_id, page_no);
        InternalPage *parent_page_after_recursion = (InternalPage *)frame->data;
        int child_index = internal_page_get_child_index_by_page(parent_page_after_recursion, child_page_no);

        BufferFrame *child_frame = buf_get_frame(space_id, child_page_no);
        if (!child_frame)
        {
            free(page_snapshot);
            return -1;
        }

        if (child_frame->page_type == PAGE_TYPE_LEAF)
        {
            if (((LeafPage *)child_frame->data)->num_records < MAX_LEAF_RECORDS / 2)
            {
                handle_leaf_underflow(space_id, page_no, parent_page_after_recursion, child_page_no, child_index);
            }
        }
        else
        {
            if (((InternalPage *)child_frame->data)->num_entries < MAX_INTERNAL_ENTRIES / 2)
            {
                handle_internal_underflow(space_id, page_no, parent_page_after_recursion, child_page_no, child_index);
            }
        }
        free(page_snapshot);
        return 0;
    }
    return -1;
}

// [重写] table_delete_row，确保逻辑完整
int table_delete_row(uint32_t space_id, uint32_t id_to_delete)
{
    BTreeMeta *meta = get_meta(space_id);
    if (!meta)
        return -1;
    uint32_t root_page_no = meta->root_page_no;
    if (root_page_no == 0)
        return -1;

    if (b_tree_delete_internal(space_id, root_page_no, id_to_delete) != 0)
    {
        return -1;
    }

    // 重新获取最新的根页号
    meta = get_meta(space_id);
    root_page_no = meta->root_page_no;
    BufferFrame *root_frame = buf_get_frame(space_id, root_page_no);

    if (root_frame->page_type == PAGE_TYPE_INTERNAL && ((InternalPage *)root_frame->data)->num_entries == 0)
    {
        uint32_t new_root_page_no = ((InternalPage *)root_frame->data)->first_child_page_no;
        meta->root_page_no = new_root_page_no;
        // buf_mark_dirty(space_id, 0);
        printf("Tree height decreased. New root is page %u\n", new_root_page_no);
    }
    return 0;
}

int table_update_row(uint32_t space_id, const Row *new_row_data)
{
    uint32_t leaf_page_no = b_tree_find_leaf_page(space_id, new_row_data->id);
    if (leaf_page_no == 0)
        return -1;

    BufferFrame *frame = buf_get_frame(space_id, leaf_page_no);
    if (!frame)
        return -1;
    LeafPage *page = (LeafPage *)frame->data;

    int record_index = -1;
    for (int i = 0; i < page->num_records; i++)
    {
        if (page->records[i].id == new_row_data->id)
        {
            record_index = i;
            break;
        }
    }
    if (record_index == -1)
        return -1;

    // [最终修复]
    if (strcmp(page->records[record_index].username, new_row_data->username) != 0)
    {
        // a. 删除旧的辅助索引条目
        if (secondary_b_tree_delete(space_id, page->records[record_index].username, page->records[record_index].id) != 0)
        {
            printf("Error: Failed to delete old secondary index entry during update.\n");
            return -1;
        }

        // b. 插入新的辅助索引条目
        SecondaryLeafEntry new_secondary_entry;
        strncpy(new_secondary_entry.key, new_row_data->username, USERNAME_MAX_LEN);
        new_secondary_entry.primary_key = new_row_data->id;
        if (secondary_b_tree_insert(space_id, &new_secondary_entry) != 0)
        {
            printf("Error: Failed to insert new secondary index entry during update.\n");
            return -1;
        }
    }

    // 更新主B+树数据
    page->records[record_index] = *new_row_data;
    // buf_mark_dirty(space_id, leaf_page_no);
    return 0;
}

// [新增] 范围查询的顶层实现
void b_tree_search_range(uint32_t space_id, uint32_t lower_bound, uint32_t upper_bound, const Statement *statement)
{
    // 1. [定位起点] 首先，找到第一个可能包含 `lower_bound` 的叶子页
    uint32_t current_page_no = b_tree_find_leaf_page(space_id, lower_bound);
    if (current_page_no == 0)
    {
        printf("No records found in the given range.\n");
        return;
    }

    int count = 0;
    // 2. [横向扫描] 开始在叶子节点链表上进行扫描
    while (current_page_no != 0)
    {
        BufferFrame *frame = buf_get_frame(space_id, current_page_no);
        if (!frame)
            break;
        LeafPage *page = (LeafPage *)frame->data;

        // 3. 在当前页内，遍历所有记录
        for (int i = 0; i < page->num_records; i++)
        {
            // 如果记录的ID在指定的 [lower_bound, upper_bound) 区间内
            if (page->records[i].id > lower_bound && page->records[i].id < upper_bound)
            {
                print_projected_row(&page->records[i], statement);
                count++;
            }
            // 如果记录的ID已经超出了上限，说明扫描可以结束了
            if (page->records[i].id >= upper_bound)
            {
                current_page_no = 0; // 设置为0，以跳出外层while循环
                break;
            }
        }

        // 如果内层循环没有break，就继续移动到下一个叶子页
        if (current_page_no != 0)
        {
            current_page_no = page->next_leaf;
        }
    }
    printf("Found %d records in range (%d, %d).\n", count, lower_bound, upper_bound);
}