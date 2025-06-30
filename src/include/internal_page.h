#ifndef INTERNAL_PAGE_H
#define INTERNAL_PAGE_H

#include <stdint.h>
#include "page.h"

typedef struct
{
    uint32_t key;           // 键，通常是子树中最小的键值
    uint32_t child_page_no; // 子节点的页号
} InternalEntry;

// 计算一个页最多能容纳多少个内节点条目
// 注意：内节点还需要一个额外的指针，指向键值小于所有条目的子节点，
// 所以 InternalPage 结构中会多一个 first_child_page_no。
// 但为了简化计算，我们这里先按条目大小估算。
#define MAX_INTERNAL_ENTRIES ((PAGE_DATA_SIZE - sizeof(uint32_t) - sizeof(uint16_t))) / sizeof(InternalEntry)

// 定义内节点页的结构
typedef struct
{
    uint16_t num_entries;                        // 当前页中的条目数
    uint32_t first_child_page_no;                // 指向第一个子节点（其键值小于所有entries中的key）
    InternalEntry entries[MAX_INTERNAL_ENTRIES]; // 条目数组
} InternalPage;

uint32_t internal_page_get_child(InternalPage *page, uint32_t key);

int internal_page_insert(InternalPage *page, uint32_t key, uint32_t child_page_no);

#endif