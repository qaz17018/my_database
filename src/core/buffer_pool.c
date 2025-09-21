#include "buffer_pool.h"
#include "hash_table.h"
#include "b_tree.h" // [新增] 引入 b_tree.h 以獲取 BTreeMeta 的定義
#include <string.h>
#include <stdio.h>
#include <time.h>

// [移除] 不再需要這個易失的全局數組
// #define MAX_SPACES 100
// static uint32_t g_next_page_no_per_space[MAX_SPACES];

// --- [innodb_old_blocks_time 实现] ---
#define LRU_OLD_PCT 37 // 老生代区域占比 37%
// 定义我们的“冷静期”为 1000 毫秒，与 MySQL 默认值一致
#define PROMOTION_RESISTANCE_MS 1000

static struct
{
    BufferFrame frames[MAX_BUFFER_POOL_PAGES];
    HashTable *hash_map;
    BufferFrame *lru_head; // 新生代头部
    BufferFrame *lru_tail; // 老生代尾部
    int size;
    BufferFrame *old_gen_head; // 指向老生代的头部（中点）
} g_buffer_pool;

static uint32_t combine_key(uint32_t space_id, uint32_t page_no)
{
    return (space_id << 24) | page_no;
}

static void lru_detach_node(BufferFrame *frame)
{
    if (frame->prev)
        frame->prev->next = frame->next;
    if (frame->next)
        frame->next->prev = frame->prev;
    if (g_buffer_pool.lru_head == frame)
        g_buffer_pool.lru_head = frame->next;
    if (g_buffer_pool.lru_tail == frame)
        g_buffer_pool.lru_tail = frame->prev;
    // 如果被移除的节点恰好是中点，我们需要找到下一个节点作为新的中点
    if (g_buffer_pool.old_gen_head == frame)
        g_buffer_pool.old_gen_head = frame->next;
    frame->prev = NULL;
    frame->next = NULL;
}

static void lru_attach_to_head(BufferFrame *frame)
{
    frame->next = g_buffer_pool.lru_head;
    if (g_buffer_pool.lru_head)
        g_buffer_pool.lru_head->prev = frame;
    g_buffer_pool.lru_head = frame;
    if (g_buffer_pool.lru_tail == NULL)
        g_buffer_pool.lru_tail = frame;
    frame->prev = NULL;
}

// 动态维护中点指针
static void maintain_old_gen_head()
{
    if (g_buffer_pool.size == 0)
    {
        g_buffer_pool.old_gen_head = NULL;
        return;
    }
    int young_len = g_buffer_pool.size * (100 - LRU_OLD_PCT) / 100;
    if (young_len == 0 && g_buffer_pool.size > 0)
        young_len = 1; // 至少有一个新生代
    if (young_len >= g_buffer_pool.size)
    {
        g_buffer_pool.old_gen_head = NULL; // 没有老生代
        return;
    }

    BufferFrame *current = g_buffer_pool.lru_head;
    for (int i = 0; i < young_len - 1; i++)
    {
        current = current->next;
    }
    g_buffer_pool.old_gen_head = current ? current->next : NULL;
}

static void lru_attach_to_midpoint(BufferFrame *frame)
{
    if (!g_buffer_pool.old_gen_head)
    { // 如果没有中点（例如链表很短），直接加到尾部
        frame->prev = g_buffer_pool.lru_tail;
        if (g_buffer_pool.lru_tail)
            g_buffer_pool.lru_tail->next = frame;
        g_buffer_pool.lru_tail = frame;
        if (g_buffer_pool.lru_head == NULL)
            g_buffer_pool.lru_head = frame;
    }
    else
    { // 插入到中点的前面
        frame->next = g_buffer_pool.old_gen_head;
        frame->prev = g_buffer_pool.old_gen_head->prev;
        if (g_buffer_pool.old_gen_head->prev)
        {
            g_buffer_pool.old_gen_head->prev->next = frame;
        }
        else
        {
            // 如果中点就是头部，那么新节点就是新的头部
            g_buffer_pool.lru_head = frame;
        }
        g_buffer_pool.old_gen_head->prev = frame;
    }
}

void buf_init()
{
    memset(g_buffer_pool.frames, 0, sizeof(g_buffer_pool.frames));
    if (g_buffer_pool.hash_map)
        hash_table_destroy(g_buffer_pool.hash_map);
    g_buffer_pool.hash_map = hash_table_create(MAX_BUFFER_POOL_PAGES * 2);
    g_buffer_pool.lru_head = NULL;
    g_buffer_pool.lru_tail = NULL;
    g_buffer_pool.size = 0;
    g_buffer_pool.old_gen_head = NULL;
    // [移除] 不再需要重置這個易失的數組
    // memset(g_next_page_no_per_space, 0, sizeof(g_next_page_no_per_space));
}

static BufferFrame *evict_frame()
{
    BufferFrame *victim = g_buffer_pool.lru_tail;
    if (!victim)
        return NULL;

    hash_table_remove(g_buffer_pool.hash_map, combine_key(victim->space_id, victim->page_no));
    lru_detach_node(victim);
    g_buffer_pool.size--;

    if (victim->is_dirty)
    {
        write_page(victim->space_id, victim->page_no, victim->page_type, victim->data, victim->lsn);
    }
    victim->is_used = 0;
    maintain_old_gen_head(); // 移除后需要重新计算中点
    return victim;
}

void buf_mark_dirty(uint32_t space_id, uint32_t page_no, uint64_t lsn) // <--- 修改签名
{
    int idx = hash_table_get(g_buffer_pool.hash_map, combine_key(space_id, page_no));
    if (idx >= 0)
    {
        g_buffer_pool.frames[idx].is_dirty = 1;
        // [修改] 只有当新的 LSN 更大时才更新，这是 ARIES 恢复算法中的一个重要细节
        if (lsn > g_buffer_pool.frames[idx].lsn)
        {
            g_buffer_pool.frames[idx].lsn = lsn;
        }
    }
}

void buf_flush_all()
{
    for (int i = 0; i < MAX_BUFFER_POOL_PAGES; i++)
    {
        if (g_buffer_pool.frames[i].is_used && g_buffer_pool.frames[i].is_dirty)
        {
            write_page(g_buffer_pool.frames[i].space_id, g_buffer_pool.frames[i].page_no, g_buffer_pool.frames[i].page_type, g_buffer_pool.frames[i].data, g_buffer_pool.frames[i].lsn);
            g_buffer_pool.frames[i].is_dirty = 0;
        }
    }
}

BufferFrame *buf_get_frame(uint32_t space_id, uint32_t page_no)
{
    int idx = hash_table_get(g_buffer_pool.hash_map, combine_key(space_id, page_no));
    if (idx >= 0)
    { // 缓存命中
        BufferFrame *frame = &g_buffer_pool.frames[idx];

        int is_old = 1;
        BufferFrame *current = g_buffer_pool.lru_head;
        while (current && current != g_buffer_pool.old_gen_head)
        {
            if (current == frame)
            {
                is_old = 0; // 它在新生代
                break;
            }
            current = current->next;
        }

        if (is_old)
        {
            // --- [innodb_old_blocks_time 逻辑] ---
            // 它在老生代。检查是否过了冷静期。
            clock_t now = clock();
            double elapsed_ms = ((double)(now - frame->first_access_time) * 1000) / CLOCKS_PER_SEC;
            if (elapsed_ms > PROMOTION_RESISTANCE_MS)
            {
                // 冷静期已过，晋升！
                lru_detach_node(frame);
                lru_attach_to_head(frame);
                maintain_old_gen_head();
            }
            // 如果没过冷静期，什么都不做，让它继续待在老生代。
        }
        else
        {
            // 它在新生代，只需移动到头部即可
            lru_detach_node(frame);
            lru_attach_to_head(frame);
            // 无需维护中点，因为新生代内部的移动不影响中点位置
        }
        return frame;
    }

    // 缓存未命中，从磁盘加载
    BufferFrame *new_frame = NULL;
    if (g_buffer_pool.size < MAX_BUFFER_POOL_PAGES)
    {
        for (int i = 0; i < MAX_BUFFER_POOL_PAGES; ++i)
        {
            if (!g_buffer_pool.frames[i].is_used)
            {
                new_frame = &g_buffer_pool.frames[i];
                break;
            }
        }
    }
    else
    {
        new_frame = evict_frame();
    }

    if (!new_frame)
    {
        return NULL;
    }

    PageHeader header;
    if (read_page(space_id, page_no, new_frame->data, &header) != 0)
    {
        new_frame->is_used = 0;
        return NULL;
    }

    new_frame->space_id = space_id;
    new_frame->page_no = page_no;
    new_frame->page_type = header.page_type;
    new_frame->is_used = 1;
    new_frame->is_dirty = 0;
    new_frame->lsn = header.page_lsn;       // [新增] 从页头恢复 page_lsn
    new_frame->first_access_time = clock(); // 记录首次访问时间

    g_buffer_pool.size++;
    lru_attach_to_midpoint(new_frame); // 新页面插入到中点
    maintain_old_gen_head();

    int frame_idx = new_frame - g_buffer_pool.frames;
    hash_table_put(g_buffer_pool.hash_map, combine_key(space_id, page_no), frame_idx);

    return new_frame;
}

// [核心改造] 重寫 buf_alloc_frame 函數
BufferFrame *buf_alloc_frame(uint32_t space_id, PageType type, uint32_t *out_page_no)
{
    // 步驟 1: 獲取元數據頁 (page 0) 來決定新的頁號
    BufferFrame *meta_frame = buf_get_frame(space_id, 0);
    if (!meta_frame)
    {
        // 這通常只會在數據庫第一次創建時發生，此時 B-Tree 結構還不存在
        // b_tree_create 函數會手動處理 0 號和 1 號頁的分配
        // 在正常運行中，meta_frame 必須存在
        fprintf(stderr, "Error: Cannot allocate page, B-Tree metadata not found for space %u.\n", space_id);
        return NULL;
    }
    BTreeMeta *meta = (BTreeMeta *)meta_frame->data;

    // 步驟 2: 決定新頁的頁號，並更新元數據
    uint32_t new_page_no = meta->total_pages;
    *out_page_no = new_page_no;

    meta->total_pages++;
    // 將元數據頁標記為髒頁，確保頁計數器的增長被持久化
    buf_mark_dirty(space_id, 0, 0);

    // 步驟 3: 為新頁在緩衝池中找到一個 Frame (沿用舊邏輯)
    BufferFrame *new_frame = NULL;
    if (g_buffer_pool.size < MAX_BUFFER_POOL_PAGES)
    {
        for (int i = 0; i < MAX_BUFFER_POOL_PAGES; ++i)
        {
            if (!g_buffer_pool.frames[i].is_used)
            {
                new_frame = &g_buffer_pool.frames[i];
                break;
            }
        }
    }
    else
    {
        new_frame = evict_frame();
    }

    if (!new_frame)
    {
        // 如果找不到可用的 frame，需要回滾計數器
        meta->total_pages--;
        return NULL;
    }

    // 步驟 4: 初始化新 Frame 的屬性
    memset(new_frame->data, 0, PAGE_DATA_SIZE);
    new_frame->space_id = space_id;
    new_frame->page_no = new_page_no;
    new_frame->is_dirty = 1; // 新分配的頁總是髒的
    new_frame->is_used = 1;
    new_frame->page_type = type;
    new_frame->first_access_time = clock();
    new_frame->lsn = 0; // 一個全新的頁面，它的初始 LSN 應該是 0

    // 步驟 5: 將新 Frame 加入 LRU 鏈表和哈希表
    g_buffer_pool.size++;
    lru_attach_to_midpoint(new_frame); // 新分配的頁也進入中點
    maintain_old_gen_head();

    int frame_idx = new_frame - g_buffer_pool.frames;
    hash_table_put(g_buffer_pool.hash_map, combine_key(space_id, new_page_no), frame_idx);

    // 注意：我們不再需要在分配時立即寫盤 (write_page)，
    // Buffer Pool 的換出機制或 `buf_flush_all` 會負責將它寫入磁盤。
    // 這一步的省略可以提升性能。

    return new_frame;
}