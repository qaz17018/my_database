#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stdint.h>
#include "page.h"
#include <time.h> // <--- 确保包含了 time.h

#define MAX_BUFFER_POOL_PAGES 64

struct BufferFrame
{
    uint32_t space_id;
    uint32_t page_no;
    PageType page_type;
    int is_dirty;
    int is_used;
    uint64_t lsn;

    // 记录页面首次被加载到缓冲池的时间点
    clock_t first_access_time;

    struct BufferFrame *prev;
    struct BufferFrame *next;

    char data[PAGE_DATA_SIZE];
};

typedef struct BufferFrame BufferFrame;

void buf_init();
BufferFrame *buf_get_frame(uint32_t space_id, uint32_t page_no);
BufferFrame *buf_alloc_frame(uint32_t space_id, PageType type, uint32_t *out_page_no);
void buf_mark_dirty(uint32_t space_id, uint32_t page_no, uint64_t lsn); // <--- 修改签名
void buf_flush_all();

#endif