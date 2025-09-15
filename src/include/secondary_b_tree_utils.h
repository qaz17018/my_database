#ifndef SECONDARY_B_TREE_UTILS_H
#define SECONDARY_B_TREE_UTILS_H

#include <stdint.h>
#include "b_tree.h"       // 需要引入它来获取Meta定义
#include "b_tree_utils.h" // 我们可以复用 BTreeStats 结构体

/**
 * @brief 遍历辅助索引B+树并打印其健康度统计信息
 * @param space_id 要分析的B+树所在的空间ID
 */
void secondary_b_tree_print_stats(uint32_t space_id);

#endif