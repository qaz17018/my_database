#ifndef B_TREE_H
#define B_TREE_H

#include "row.h"

typedef struct
{
    uint32_t root_page_no;              // 根节点地页号
    uint32_t username_idx_root_page_no; // username辅助索引B+树的根节点页号
    uint32_t total_pages;               // 树的总页数
} BTreeMeta;

int table_insert_row(uint32_t space_id, const Row *row);

uint32_t table_search_pk_by_username(uint32_t space_id, const char *username);

int b_tree_create(uint32_t space_id);
int b_tree_insert(uint32_t space_id, const Row *row);

// int b_tree_insert(uint32_t space_id, uint32_t root_page_no, const Row *row);

#endif