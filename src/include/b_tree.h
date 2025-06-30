#ifndef B_TREE_H
#define B_TREE_H

#include "row.h"

int b_tree_insert(uint32_t space_id, uint32_t root_page_no, const Row *row);

#endif