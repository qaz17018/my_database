#ifndef B_TREE_UTILS_H
#define B_TREE_UTILS_H

#include <stdint.h>

// 定义用于存储B+树统计信息的结构体
typedef struct
{
    uint32_t height;
    uint32_t node_count;
    uint32_t leaf_node_count;
    uint32_t internal_node_count;
    uint64_t record_count;
    double total_leaf_fullness;
    double avg_leaf_fullness;
} BTreeStats;

/**
 * @brief 遍历B+树并打印其健康度统计信息
 * @param space_id 要分析的B+树所在的空间ID
 */
void b_tree_print_stats(uint32_t space_id);

#endif