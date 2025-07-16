#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stdint.h>
#include "page.h"

#define MAX_BUFFER_POOL_PAGES 64

// Step 1: Define the structure.
struct BufferFrame
{
    uint32_t space_id;
    uint32_t page_no;
    PageType page_type;
    int is_dirty;
    int is_used;

    // Self-referential pointers for the doubly linked list.
    struct BufferFrame *prev;
    struct BufferFrame *next;

    char data[PAGE_DATA_SIZE];
};

// Step 2: Create a type alias for the structure.
typedef struct BufferFrame BufferFrame;

/* typedef struct
{
    uint32_t space_id;
    uint32_t page_no;
    PageType page_type;
    int is_dirty;
    int is_used;

    // LRU算法优化，引入哈希表和双向链表，表头最新，表尾最老
    // 指向链表的前一个和后一个节点
    struct BufferFrame *prev;
    struct BufferFrame *next;

    // uint64_t last_access_time;
    char data[PAGE_DATA_SIZE];
} BufferFrame; */

void buf_init();

// [接口升级] 不再返回void*，而是返回整个缓冲块的指针
BufferFrame *buf_get_frame(uint32_t space_id, uint32_t page_no);
BufferFrame *buf_alloc_frame(uint32_t space_id, PageType type, uint32_t *out_page_no);

void *buf_get_page(uint32_t space_id, uint32_t page_no, PageType type);
void buf_mark_dirty(uint32_t space_id, uint32_t page_no);
void buf_flush_all();

void *buf_alloc_page(uint32_t space_id, PageType type, uint32_t *out_page_no);

#endif