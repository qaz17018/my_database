#ifndef SECONDARY_INDEX_PAGE_H
#define SECONDARY_INDEX_PAGE_H

#include "row.h"
#include "page.h"

// --- 辅助索引的叶子节点 ---

// 叶子节点中存储的条目：(键 + 主键)
typedef struct
{
    char key[USERNAME_MAX_LEN]; // 这里的键是 username
    uint32_t primary_key;       // 指向主B+树中数据行的主键ID
} SecondaryLeafEntry;

// 计算一个叶子页最多能容纳多少个辅助索引条目
#define MAX_SECONDARY_LEAF_ENTRIES (PAGE_DATA_SIZE / sizeof(SecondaryLeafEntry))

// 辅助索引叶子页的结构
typedef struct
{
    uint32_t next_leaf; // 指向下一个兄弟叶子页
    uint16_t num_entries;
    SecondaryLeafEntry entries[MAX_SECONDARY_LEAF_ENTRIES];
} SecondaryLeafPage;

// --- 辅助索引的内节点 (可以复用) ---
// 辅助索引的内节点结构，和我们主B+树的内节点结构是完全一样的。
// 它们都只存储 (键 + 指向下一层节点的指针)。
// 为了清晰，我们可以在这里重新定义，或者直接复用 internal_page.h 中的定义。
// 这里我们选择重新定义，让模块更独立。

typedef struct
{
    char key[USERNAME_MAX_LEN];
    uint32_t child_page_no;
} SecondaryInternalEntry;

#define MAX_SECONDARY_INTERNAL_ENTRIES ((PAGE_DATA_SIZE - sizeof(uint32_t) - sizeof(uint16_t)) / sizeof(SecondaryInternalEntry))

typedef struct
{
    uint16_t num_entries;
    uint32_t first_child_page_no;
    SecondaryInternalEntry entries[MAX_SECONDARY_INTERNAL_ENTRIES];
} SecondaryInternalPage;

#endif