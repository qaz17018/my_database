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

int internal_page_insert(uint32_t space_id, uint32_t page_no, InternalPage *page, uint32_t key, uint32_t child_page_no);

/**
 * @brief 向内节点插入条目，如果页满则进行分裂
 * * @param space_id 空间ID
 * @param page_no 当前内节点的页号
 * @param key 要插入的键
 * @param child_page_no 要插入的键所指向的子节点页号
 * @param out_split_key 如果发生分裂，这里会传出被推向上一层的新分裂键
 * @param out_new_page_no 如果发生分裂，这里会传出新分裂出的内节点的页号
 * @return int 0表示成功
 */
int internal_page_insert_or_split(uint32_t space_id, uint32_t page_no, uint32_t key, uint32_t child_page_no, uint32_t *out_split_key, uint32_t *out_new_page_no);

// [替换] 用更准确的函数替换
int internal_page_get_child_index_by_page(InternalPage *page, uint32_t child_page_no);

// ...
// [新增] 声明从内节点删除条目的函数
void internal_page_delete_entry(uint32_t space_id, uint32_t page_no, InternalPage *page, int entry_index);

#endif