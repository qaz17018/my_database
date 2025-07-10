#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stdint.h>
#include "page.h"

#define MAX_BUFFER_POOL_PAGES 64

typedef struct
{
    uint32_t space_id;
    uint32_t page_no;
    PageType page_type;
    int is_dirty;
    int is_used;
    uint64_t last_access_time;
    char data[PAGE_DATA_SIZE];
} BufferFrame;

void buf_init();

// [接口升级] 不再返回void*，而是返回整个缓冲块的指针
BufferFrame *buf_get_frame(uint32_t space_id, uint32_t page_no);
BufferFrame *buf_alloc_frame(uint32_t space_id, PageType type, uint32_t *out_page_no);

void *buf_get_page(uint32_t space_id, uint32_t page_no, PageType type);
void buf_mark_dirty(uint32_t space_id, uint32_t page_no);
void buf_flush_all();

void *buf_alloc_page(uint32_t space_id, PageType type, uint32_t *out_page_no);

#endif