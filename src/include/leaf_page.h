#ifndef LEAF_PAGE_H
#define LEAF_PAGE_H

#include <stdint.h>
#include "row.h"

#define MAX_LEAF_RECORDS 256

typedef struct
{
    uint32_t next_leaf;
    uint16_t num_records;
    Row records[MAX_LEAF_RECORDS];
} LeafPage;

int leaf_page_insert(uint32_t space_id, uint32_t page_no, LeafPage *page, const Row *row);

int leaf_page_insert_or_split(uint32_t space_id, uint32_t page_no, const Row *row, uint32_t *out_new_page_no, uint32_t *out_split_key);

Row *leaf_page_search(LeafPage *page, uint32_t id);

#endif