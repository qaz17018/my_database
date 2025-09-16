#ifndef SECONDARY_B_TREE_H
#define SECONDARY_B_TREE_H

#include "secondary_index_page.h"

// --- 公共函数声明 ---
// 这两个函数是 secondary_b_tree.c 模块希望暴露给外部使用的接口

uint32_t secondary_b_tree_search(uint32_t space_id, const char *key);
int secondary_b_tree_insert(uint32_t space_id, const SecondaryLeafEntry *entry);

// [新增] 声明辅助索引的顶层删除函数
int secondary_b_tree_delete(uint32_t space_id, const char *key, uint32_t primary_key);

static int secondary_internal_page_insert(uint32_t space_id, uint32_t page_no, SecondaryInternalPage *page, const char *key, uint32_t child_page_no);

#endif