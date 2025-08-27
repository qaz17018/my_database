#include "b_tree.h"
#include "buffer_pool.h"
#include "leaf_page.h"
#include "internal_page.h"
#include "secondary_b_tree.h"
#include <stdio.h>
#include <string.h>

static uint32_t b_tree_find_leaf_page_internal(uint32_t space_id, uint32_t page_no, uint32_t id);
static uint32_t b_tree_find_leaf_page(uint32_t space_id, uint32_t id);

static BTreeMeta *get_meta(uint32_t space_id)
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
    meta->total_pages = 2; // 现在我们有了0号（meta）和1号（root）两个页

    // 4. 将元数据的修改登记在案（标记为脏页），以便写回磁盘
    buf_mark_dirty(space_id, meta_page_no);

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

        // [核心修复-3] 在成功分配新根页后，更新元数据
        BufferFrame *meta_frame_for_update = buf_get_frame(space_id, 0);
        if (meta_frame_for_update)
        {
            BTreeMeta *meta_for_update = (BTreeMeta *)meta_frame_for_update->data;
            meta_for_update->total_pages++;
            // root_page_no 的更新会在下面完成，这里先标记脏
            buf_mark_dirty(space_id, 0);
        }

        // 新根节点指向原来的老根（现在是左孩子）和分裂出的新页（右孩子）
        new_root->first_child_page_no = root_page_no;
        new_root->entries[0].key = split_key;
        new_root->entries[0].child_page_no = new_page_no;
        new_root->num_entries = 1;

        // 更新meta信息，指向新的根
        meta->root_page_no = new_root_page_no;
        buf_mark_dirty(space_id, 0);
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

// [核心修改] 用我们新的追踪和擦除功能，重写顶层删除函数
int table_delete_row(uint32_t space_id, uint32_t id_to_delete)
{
    // TODO: 完整的删除，还需要同步删除所有辅助索引。我们先简化，只删除主索引。

    // 1. 调用我们的新“追踪”函数，找到包含目标id的叶子页的页号
    uint32_t leaf_page_no = b_tree_find_leaf_page(space_id, id_to_delete);

    if (leaf_page_no == 0)
    {
        printf("Row with id %d not found in any leaf page.\n", id_to_delete);
        return -1; // 未找到记录
    }

    // 2. 获取该叶子页的内存指针
    BufferFrame *frame = buf_get_frame(space_id, leaf_page_no);
    if (!frame)
        return -1;
    LeafPage *page = (LeafPage *)frame->data;

    // 3. 调用我们的新“擦除”函数，从这个具体的叶子页中删除记录
    if (leaf_page_delete(space_id, leaf_page_no, page, id_to_delete) == 0)
    {
        return 0; // 成功
    }
    else
    {
        // 理论上，如果find_leaf_page找到了页，这里不应该失败，但做好保护
        printf("Row with id %d found on page %u, but failed to delete.\n", id_to_delete, leaf_page_no);
        return -1; // 删除失败
    }
}