#include <stdio.h>
#include <string.h>
#include "b_tree_utils.h"
#include "b_tree.h"
#include "buffer_pool.h"
#include "leaf_page.h"
#include "internal_page.h"

// 递归遍历函数，用于收集统计信息
static void b_tree_traverse_for_stats(uint32_t space_id, uint32_t page_no, uint32_t current_depth, BTreeStats *stats)
{
    if (page_no == 0)
        return;

    stats->node_count++;
    if (current_depth > stats->height)
    {
        stats->height = current_depth;
    }

    BufferFrame *frame = buf_get_frame(space_id, page_no);
    if (!frame)
        return;

    if (frame->page_type == PAGE_TYPE_LEAF)
    {
        stats->leaf_node_count++;
        LeafPage *page = (LeafPage *)frame->data;
        stats->record_count += page->num_records;
        stats->total_leaf_fullness += (double)page->num_records / MAX_LEAF_RECORDS;
        return;
    }

    if (frame->page_type == PAGE_TYPE_INTERNAL)
    {
        stats->internal_node_count++;

        // --- 核心修复 ---
        // 1. 在栈上创建一个 page 的安全副本
        InternalPage page_copy;
        memcpy(&page_copy, frame->data, sizeof(InternalPage));
        // -----------------

        // 2. 后续的所有操作，都使用这个绝对安全的 page_copy
        b_tree_traverse_for_stats(space_id, page_copy.first_child_page_no, current_depth + 1, stats);
        for (int i = 0; i < page_copy.num_entries; i++)
        {
            b_tree_traverse_for_stats(space_id, page_copy.entries[i].child_page_no, current_depth + 1, stats);
        }
    }
}

// 顶层入口函数
void b_tree_print_stats(uint32_t space_id)
{
    BTreeMeta *meta = get_meta(space_id);
    if (!meta || meta->root_page_no == 0)
    {
        printf("B-Tree for space %u is empty or does not exist.\n", space_id);
        return;
    }

    BTreeStats stats;
    memset(&stats, 0, sizeof(stats));

    b_tree_traverse_for_stats(space_id, meta->root_page_no, 1, &stats);

    if (stats.leaf_node_count > 0)
    {
        stats.avg_leaf_fullness = (stats.total_leaf_fullness / stats.leaf_node_count) * 100.0;
    }

    printf("\n--- B-Tree Health Report (Space ID: %u) ---\n", space_id);
    printf("     Tree Height: %u\n", stats.height);
    printf("     Total Nodes: %u (Internal: %u, Leaf: %u)\n",
           stats.node_count, stats.internal_node_count, stats.leaf_node_count);
    printf("   Total Records: %lu\n", stats.record_count);
    printf("Avg Leaf Fullness: %.2f%%\n", stats.avg_leaf_fullness);
    printf("-------------------------------------------\n\n");
}