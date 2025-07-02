#ifndef B_TREE_H
#define B_TREE_H

#include "row.h"

typedef struct
{
    uint32_t root_page_no; // 根节点地页号
    uint32_t total_pages;  // 树的总页数
} BTreeMeta;

int b_tree_create(uint32_t space_id);
// int b_tree_insert(uint32_t space_id, const Row *row);

int b_tree_insert(uint32_t space_id, uint32_t root_page_no, const Row *row);

#endif