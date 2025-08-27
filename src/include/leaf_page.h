#ifndef LEAF_PAGE_H
#define LEAF_PAGE_H

#include <stdint.h>
#include "row.h"
#include "page.h" // [新增] 需要引入page.h来获取PAGE_DATA_SIZE

// [核心修复] 动态计算一页最多能容纳的行数
#define MAX_LEAF_RECORDS ((PAGE_DATA_SIZE - sizeof(uint32_t) - sizeof(uint16_t)) / sizeof(Row))

typedef struct
{
    uint32_t next_leaf;
    uint16_t num_records;
    Row records[MAX_LEAF_RECORDS];
} LeafPage;

int leaf_page_insert(uint32_t space_id, uint32_t page_no, LeafPage *page, const Row *row);

int leaf_page_insert_or_split(uint32_t space_id, uint32_t page_no, const Row *row, uint32_t *out_new_page_no, uint32_t *out_split_key);

Row *leaf_page_search(LeafPage *page, uint32_t id);

int leaf_page_delete(uint32_t space_id, uint32_t page_no, LeafPage *page, uint32_t id);

#endif