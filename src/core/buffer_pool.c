#include "buffer_pool.h"
#include "hash_table.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// --- 高性能LRU缓存结构 ---

static struct
{
    BufferFrame frames[MAX_BUFFER_POOL_PAGES];
    HashTable *hash_map;
    BufferFrame *lru_head;
    BufferFrame *lru_tail;
} g_buffer_pool;

#define MAX_SPACES 100
static uint32_t g_next_page_no_per_space[MAX_SPACES];

// 组合 space_id 和 page_no 成为一个唯一的32位键
// 这里我们假设 space_id 不会太大
static uint32_t combine_key(uint32_t space_id, uint32_t page_no)
{
    return (space_id << 24) | page_no;
}

// 将一个节点从链表中“摘下”
static void lru_detach_node(BufferFrame *frame)
{
    if (frame->prev)
    {
        frame->prev->next = frame->next;
    }
    if (frame->next)
    {
        frame->next->prev = frame->prev;
    }
    if (g_buffer_pool.lru_head == frame)
    {
        g_buffer_pool.lru_head = frame->next;
    }
    if (g_buffer_pool.lru_tail == frame)
    {
        g_buffer_pool.lru_tail = frame->prev;
    }
    frame->prev = NULL;
    frame->next = NULL;
}

// 将一个节点移动到链表头部（标记为最新）
static void lru_attach_to_head(BufferFrame *frame)
{
    frame->next = g_buffer_pool.lru_head;
    if (g_buffer_pool.lru_head)
    {
        g_buffer_pool.lru_head->prev = frame;
    }
    g_buffer_pool.lru_head = frame;
    if (g_buffer_pool.lru_tail == NULL)
    {
        g_buffer_pool.lru_tail = frame;
    }
    frame->prev = NULL;
}

// --- 缓冲池主逻辑重构
void buf_init()
{
    memset(g_buffer_pool.frames, 0, sizeof(g_buffer_pool.frames));
    if (g_buffer_pool.hash_map)
    {
        hash_table_destroy(g_buffer_pool.hash_map);
    }
    g_buffer_pool.hash_map = hash_table_create(MAX_BUFFER_POOL_PAGES * 2);
    g_buffer_pool.lru_head = NULL;
    g_buffer_pool.lru_tail = NULL;

    // --- [THE FIX - Step 2] ---
    // When the buffer pool is initialized, reset all page counters to 0.
    memset(g_next_page_no_per_space, 0, sizeof(g_next_page_no_per_space));
    // --- [END OF FIX - Step 2] ---
}

static int find_frame(uint32_t space_id, uint32_t page_no)
{
    for (int i = 0; i < MAX_BUFFER_POOL_PAGES; i++)
    {
        if (g_buffer_pool.frames[i].is_used &&
            g_buffer_pool.frames[i].space_id == space_id &&
            g_buffer_pool.frames[i].page_no == page_no)
        {
            return i;
        }
    }
    return -1;
}

static BufferFrame *evict_frame()
{
    BufferFrame *victim = g_buffer_pool.lru_tail;
    if (!victim)
    {
        for (int i = 0; i < MAX_BUFFER_POOL_PAGES; ++i)
        {
            if (!g_buffer_pool.frames[i].is_used)
                return &g_buffer_pool.frames[i];
        }
        return NULL;
    }

    hash_table_remove(g_buffer_pool.hash_map, combine_key(victim->space_id, victim->page_no));

    lru_detach_node(victim);

    if (victim->is_dirty)
    {
        write_page(victim->space_id, victim->page_no, victim->page_type, victim->data);
    }
    victim->is_used = 0;
    return victim;
}

void buf_mark_dirty(uint32_t space_id, uint32_t page_no)
{
    int idx = hash_table_get(g_buffer_pool.hash_map, combine_key(space_id, page_no));
    if (idx >= 0)
    {
        g_buffer_pool.frames[idx].is_dirty = 1;
    }
}

void buf_flush_all()
{
    for (int i = 0; i < MAX_BUFFER_POOL_PAGES; i++)
    {
        if (g_buffer_pool.frames[i].is_used && g_buffer_pool.frames[i].is_dirty)
        {
            write_page(g_buffer_pool.frames[i].space_id,
                       g_buffer_pool.frames[i].page_no,
                       g_buffer_pool.frames[i].page_type,
                       g_buffer_pool.frames[i].data);
            g_buffer_pool.frames[i].is_dirty = 0;
        }
    }
}

// [实现新接口] 返回 BufferFrame*
// 获取页面操作
BufferFrame *buf_get_frame(uint32_t space_id, uint32_t page_no)
{
    // O(1) 查找
    int idx = hash_table_get(g_buffer_pool.hash_map, combine_key(space_id, page_no));
    if (idx >= 0)
    {
        BufferFrame *frame = &g_buffer_pool.frames[idx];
        // 移动到链表头部
        lru_detach_node(frame);
        lru_attach_to_head(frame);
        return frame;
    }

    // --- 未命中，需要从磁盘加载 ---

    // 找一个空闲或被淘汰的缓冲块
    BufferFrame *new_frame = NULL;
    for (int i = 0; i < MAX_BUFFER_POOL_PAGES; ++i)
    {
        if (!g_buffer_pool.frames[i].is_used)
        {
            new_frame = &g_buffer_pool.frames[i];
            break;
        }
    }
    if (!new_frame)
    { // 如果没有空闲的，就淘汰一个
        new_frame = evict_frame();
    }

    PageHeader header;
    if (read_page(space_id, page_no, new_frame->data, &header) != 0)
    {
        return NULL; // 读取失败
    }

    // 配置新加载的缓冲块
    new_frame->space_id = space_id;
    new_frame->page_no = page_no;
    new_frame->page_type = header.page_type;
    new_frame->is_used = 1;
    new_frame->is_dirty = 0;

    // 放入哈希表和链表头部
    int frame_idx = new_frame - g_buffer_pool.frames;
    hash_table_put(g_buffer_pool.hash_map, combine_key(space_id, page_no), frame_idx);
    lru_attach_to_head(new_frame);

    return new_frame;
}

// [实现新接口] 返回 BufferFrame*
// 分配新页操作
BufferFrame *buf_alloc_frame(uint32_t space_id, PageType type, uint32_t *out_page_no)
{
    // --- [THE FIX - Step 3] ---
    // The global static counter is replaced by our new space-aware array.
    // Ensure we don't exceed our max supported spaces.
    if (space_id >= MAX_SPACES)
    {
        fprintf(stderr, "Error: space_id %u exceeds MAX_SPACES\n", space_id);
        return NULL;
    }

    // Get the next page number for THIS SPECIFIC space_id and then increment it.
    uint32_t page_no = g_next_page_no_per_space[space_id]++;
    *out_page_no = page_no;
    // --- [END OF FIX - Step 3] ---

    // Find a free or evictable buffer frame (this logic remains the same)
    BufferFrame *new_frame = NULL;
    for (int i = 0; i < MAX_BUFFER_POOL_PAGES; ++i)
    {
        if (!g_buffer_pool.frames[i].is_used)
        {
            new_frame = &g_buffer_pool.frames[i];
            break;
        }
    }
    if (!new_frame)
    {
        new_frame = evict_frame();
    }

    // Configure the new frame (this logic remains the same)
    memset(new_frame->data, 0, PAGE_DATA_SIZE);
    new_frame->space_id = space_id;
    new_frame->page_no = page_no;
    new_frame->is_dirty = 1;
    new_frame->is_used = 1;
    new_frame->page_type = type;

    // Add to hash table and LRU list (this logic remains the same)
    int frame_idx = new_frame - g_buffer_pool.frames;
    hash_table_put(g_buffer_pool.hash_map, combine_key(space_id, page_no), frame_idx);
    lru_attach_to_head(new_frame);

    // This immediate write is inefficient, but we'll optimize it later.
    // For now, it's needed for correctness in our simple model.
    write_page(space_id, page_no, type, new_frame->data);

    return new_frame;
}