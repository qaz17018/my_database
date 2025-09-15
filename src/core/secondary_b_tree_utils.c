#include <stdio.h>
#include <string.h>
#include "secondary_b_tree_utils.h"
#include "secondary_b_tree.h"
#include "secondary_index_page.h"
#include "buffer_pool.h"

// 递归遍历函数，用于收集辅助索引的统计信息
static void secondary_b_tree_traverse_for_stats(uint32_t space_id, uint32_t page_no, uint32_t current_depth, BTreeStats *stats)
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

    if (frame->page_type == PAGE_TYPE_SECONDARY_LEAF)
    {
        stats->leaf_node_count++;
        SecondaryLeafPage *page = (SecondaryLeafPage *)frame->data;
        stats->record_count += page->num_entries;
        stats->total_leaf_fullness += (double)page->num_entries / MAX_SECONDARY_LEAF_ENTRIES;
        return;
    }

    if (frame->page_type == PAGE_TYPE_SECONDARY_INTERNAL)
    {
        stats->internal_node_count++;
        SecondaryInternalPage *page = (SecondaryInternalPage *)frame->data;

        secondary_b_tree_traverse_for_stats(space_id, page->first_child_page_no, current_depth + 1, stats);
        for (int i = 0; i < page->num_entries; i++)
        {
            secondary_b_tree_traverse_for_stats(space_id, page->entries[i].child_page_no, current_depth + 1, stats);
        }
    }
}

// 顶层入口函数
void secondary_b_tree_print_stats(uint32_t space_id)
{
    BTreeMeta *meta = get_meta(space_id);
    if (!meta || meta->username_idx_root_page_no == 0)
    {
        printf("Secondary B-Tree for space %u is empty or does not exist.\n", space_id);
        return;
    }

    BTreeStats stats;
    memset(&stats, 0, sizeof(stats));

    secondary_b_tree_traverse_for_stats(space_id, meta->username_idx_root_page_no, 1, &stats);

    if (stats.leaf_node_count > 0)
    {
        stats.avg_leaf_fullness = (stats.total_leaf_fullness / stats.leaf_node_count) * 100.0;
    }

    printf("\n--- Secondary B-Tree Health Report (Space ID: %u) ---\n", space_id);
    printf("     Tree Height: %u\n", stats.height);
    printf("     Total Nodes: %u (Internal: %u, Leaf: %u)\n",
           stats.node_count, stats.internal_node_count, stats.leaf_node_count);
    printf("   Total Records: %lu\n", stats.record_count);
    printf("Avg Leaf Fullness: %.2f%%\n", stats.avg_leaf_fullness);
    printf("---------------------------------------------------\n\n");
}