#ifndef B_TREE_H
#define B_TREE_H

#include "row.h"
#include "sql.h"

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

Row *b_tree_search(uint32_t space_id, uint32_t id);

// [新增] 声明我们新的顶层删除函数
int table_delete_row(uint32_t space_id, uint32_t id_to_delete);

// int b_tree_insert(uint32_t space_id, uint32_t root_page_no, const Row *row);

int b_tree_delete_internal(uint32_t space_id, uint32_t page_no, uint32_t id);

// [核心修复] 在这里声明 get_meta 函数，让其他模块可以安全调用
BTreeMeta *get_meta(uint32_t space_id);

int table_update_row(uint32_t space_id, const Row *new_row);

// [修改] 让范围查询能接收 Statement
void b_tree_search_range(uint32_t space_id, uint32_t lower_bound, uint32_t upper_bound, const Statement *statement);

#endif