#include "b_tree.h"
#include "buffer_pool.h"
#include "leaf_page.h"
#include "internal_page.h"
#include <stdio.h>
#include <string.h>

static BTreeMeta *get_meta(uint32_t space_id)
{
    BufferFrame *frame = buf_get_frame(space_id, 0);
    return frame ? (BTreeMeta *)frame->data : NULL;
}

int b_tree_create(uint32_t space_id)
{
    // 1. 分配一个页作为B+树的第一个节点（它既是根，也是叶子）
    uint32_t root_page_no;
    BufferFrame *root_frame = buf_alloc_frame(space_id, PAGE_TYPE_LEAF, &root_page_no);
    if (!root_frame)
        return -1;

    // 2. 获取元数据页（此时文件已创建，可以安全获取）
    BTreeMeta *meta = get_meta(space_id);
    if (!meta)
        return -1;

    // 3. 更新元数据，指向新的根节点
    meta->root_page_no = root_page_no;
    meta->total_pages = 2; // meta page + root page

    buf_mark_dirty(space_id, 0); // 标记元数据页为脏
    printf("B+Tree created for space %u. Root is a LEAF page: %u\n", space_id, root_page_no);
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

    b_tree_insert_internal(space_id, root_page_no, row, &split_key, &new_page_no);

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