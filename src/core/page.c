#include "page.h"
#include <stdio.h>
#include <string.h>
#include "io_stats.h"

// --- [OPTIMIZATION 2 - Step 1: File Handle Cache Implementation] ---
#define MAX_OPEN_FILES 10 // Let's keep up to 10 files open at a time

// Structure to hold a cached file handle and its space_id
typedef struct
{
    uint32_t space_id;
    FILE *fp;
    long long last_used; // A simple counter for LRU
} FileHandleCacheEntry;

static FileHandleCacheEntry g_file_cache[MAX_OPEN_FILES];
static long long g_lru_counter = 0; // Global counter to track usage order

// Function to get a file handle from the cache or open a new one
static FILE *get_file_handle(uint32_t space_id, const char *mode)
{
    g_lru_counter++;
    int empty_slot = -1;
    int oldest_idx = -1;
    long long oldest_lru = -1;

    // 1. Try to find the file handle in the cache
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (g_file_cache[i].fp != NULL)
        {
            if (g_file_cache[i].space_id == space_id)
            {
                g_file_cache[i].last_used = g_lru_counter;
                return g_file_cache[i].fp;
            }
            if (oldest_lru == -1 || g_file_cache[i].last_used < oldest_lru)
            {
                oldest_lru = g_file_cache[i].last_used;
                oldest_idx = i;
            }
        }
        else if (empty_slot == -1)
        {
            empty_slot = i;
        }
    }

    // 2. 决定要使用的槽位：优先用空槽，否则用最老的槽
    int slot_to_use = (empty_slot != -1) ? empty_slot : oldest_idx;

    // 3. [THE FIX] 如果我们决定重用一个旧槽，必须先关闭并清空它
    if (g_file_cache[slot_to_use].fp != NULL)
    {
        g_io_stats.fclose_calls++; // Count the eviction
        fclose(g_file_cache[slot_to_use].fp);
        // 清理槽位，确保状态一致性
        g_file_cache[slot_to_use].fp = NULL;
        g_file_cache[slot_to_use].space_id = 0;
        g_file_cache[slot_to_use].last_used = 0;
    }

    // 4. 现在可以安全地打开新文件了
    char filename[256];
    sprintf(filename, "table_%u.db", space_id);
    g_io_stats.fopen_calls++; // Count the actual open
    FILE *new_fp = fopen(filename, mode);

    // 5. 如果打开成功，填充槽位
    if (new_fp)
    {
        g_file_cache[slot_to_use].space_id = space_id;
        g_file_cache[slot_to_use].fp = new_fp;
        g_file_cache[slot_to_use].last_used = g_lru_counter;
    }

    return new_fp;
}

// Function to initialize the cache
void file_cache_init()
{
    memset(g_file_cache, 0, sizeof(g_file_cache));
}

// Function to close all cached file handles on shutdown
void file_cache_flush_all()
{
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (g_file_cache[i].fp != NULL)
        {
            g_io_stats.fclose_calls++;
            fclose(g_file_cache[i].fp);
            g_file_cache[i].fp = NULL;
        }
    }
    file_cache_init(); // 清空所有状态
}

static long long page_offset(uint32_t page_no)
{
    return (long long)page_no * PAGE_SIZE;
}

int write_page(uint32_t space_id, uint32_t page_no, PageType type, const void *data)
{
    FILE *fp = get_file_handle(space_id, "r+b");
    if (!fp)
    {
        fp = get_file_handle(space_id, "w+b");
        if (!fp)
            return -1;
    }

    if (fseek(fp, page_offset(page_no), SEEK_SET) != 0)
    {
        return -2;
    }

    PageHeader header = {space_id, page_no, type, 0};
    g_io_stats.fwrite_calls++;
    fwrite(&header, 1, PAGE_HEADER_SIZE, fp);
    fwrite(data, 1, PAGE_DATA_SIZE, fp);

    return 0;
}

int read_page(uint32_t space_id, uint32_t page_no, void *buf, PageHeader *out_header)
{
    FILE *fp = get_file_handle(space_id, "rb");
    if (!fp)
    {
        return -1;
    }

    if (fseek(fp, page_offset(page_no), SEEK_SET) != 0)
    {
        return -2;
    }

    g_io_stats.fread_calls++;
    if (fread(out_header, 1, PAGE_HEADER_SIZE, fp) != PAGE_HEADER_SIZE)
    {
        return -3;
    }
    if (fread(buf, 1, PAGE_DATA_SIZE, fp) != PAGE_DATA_SIZE)
    {
        return -3;
    }

    return 0;
}