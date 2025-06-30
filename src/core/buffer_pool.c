#include "buffer_pool.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static struct
{
    BufferFrame frames[MAX_BUFFER_POOL_PAGES];
} pool;

void buf_init()
{
    memset(&pool, 0, sizeof(pool));
}

static int find_frame(uint32_t space_id, uint32_t page_no)
{
    for (int i = 0; i < MAX_BUFFER_POOL_PAGES; i++)
    {
        if (pool.frames[i].is_used &&
            pool.frames[i].space_id == space_id &&
            pool.frames[i].page_no == page_no)
        {
            return i;
        }
    }
    return -1;
}

static int evict_frame()
{
    int oldest = -1;
    uint64_t oldest_time = (uint64_t)-1;
    for (int i = 0; i < MAX_BUFFER_POOL_PAGES; i++)
    {
        if (!pool.frames[i].is_used)
            return i;
        if (pool.frames[i].last_access_time < oldest_time)
        {
            oldest_time = pool.frames[i].last_access_time;
            oldest = i;
        }
    }
    if (pool.frames[oldest].is_dirty)
    {
        write_page(pool.frames[oldest].space_id,
                   pool.frames[oldest].page_no,
                   pool.frames[oldest].page_type,
                   pool.frames[oldest].data);
    }
    return oldest;
}

void *buf_get_page(uint32_t space_id, uint32_t page_no, PageType type)
{
    int idx = find_frame(space_id, page_no);
    if (idx >= 0)
    {
        pool.frames[idx].last_access_time = time(NULL);
        return pool.frames[idx].data;
    }

    int new_idx = evict_frame();
    PageHeader header;
    read_page(space_id, page_no, pool.frames[new_idx].data, &header);

    pool.frames[new_idx].space_id = space_id;
    pool.frames[new_idx].page_no = page_no;
    pool.frames[new_idx].page_type = header.page_type;
    pool.frames[new_idx].is_used = 1;
    pool.frames[new_idx].is_dirty = 0;
    pool.frames[new_idx].last_access_time = time(NULL);

    return pool.frames[new_idx].data;
}

void buf_mark_dirty(uint32_t space_id, uint32_t page_no)
{
    int idx = find_frame(space_id, page_no);
    if (idx >= 0)
    {
        pool.frames[idx].is_dirty = 1;
    }
}

void buf_flush_all()
{
    for (int i = 0; i < MAX_BUFFER_POOL_PAGES; i++)
    {
        if (pool.frames[i].is_used && pool.frames[i].is_dirty)
        {
            write_page(pool.frames[i].space_id,
                       pool.frames[i].page_no,
                       pool.frames[i].page_type,
                       pool.frames[i].data);
            pool.frames[i].is_dirty = 0;
        }
    }
}

void *buf_alloc_page(uint32_t space_id, PageType type, uint32_t *out_page_no)
{
    static uint32_t next_free_page = 1;

    uint32_t page_no = next_free_page++;
    *out_page_no = page_no;

    int idx = evict_frame();

    BufferFrame *frame = &pool.frames[idx];

    memset(frame->data, 0, PAGE_DATA_SIZE);

    frame->page_no = page_no;
    frame->space_id = space_id;
    frame->is_dirty = 1;
    frame->last_access_time = time(NULL);
    frame->is_used = 1;

    write_page(space_id, page_no, type, frame->data);

    return frame->data;
}