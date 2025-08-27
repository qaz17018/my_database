#ifndef SECONDARY_B_TREE_H
#define SECONDARY_B_TREE_H

#include "secondary_index_page.h"

// --- 公共函数声明 ---
// 这两个函数是 secondary_b_tree.c 模块希望暴露给外部使用的接口

uint32_t secondary_b_tree_search(uint32_t space_id, const char *key);
int secondary_b_tree_insert(uint32_t space_id, const SecondaryLeafEntry *entry);

#endif