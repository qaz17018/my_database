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
    int found_idx = -1;
    int oldest_idx = -1;
    long long oldest_lru = -1;

    // 1. Try to find the file handle in the cache
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (g_file_cache[i].fp != NULL && g_file_cache[i].space_id == space_id)
        {
            found_idx = i;
            g_file_cache[i].last_used = g_lru_counter; // Update LRU timestamp
            return g_file_cache[i].fp;
        }
        // While searching, also find an empty slot or the oldest entry to evict
        if (g_file_cache[i].fp == NULL && oldest_idx == -1)
        {
            oldest_idx = i; // Found an empty slot
        }
        else if (oldest_lru == -1 || g_file_cache[i].last_used < oldest_lru)
        {
            oldest_lru = g_file_cache[i].last_used;
            oldest_idx = i;
        }
    }

    // 2. If not found, we need to open it. First, find a slot.
    int slot_to_use = (oldest_idx != -1) ? oldest_idx : 0;

    // If the slot we're about to use is already open, close the old file
    if (g_file_cache[slot_to_use].fp != NULL)
    {
        g_io_stats.fclose_calls++; // Count the eviction
        fclose(g_file_cache[slot_to_use].fp);
    }

    // 3. Open the new file
    char filename[256];
    sprintf(filename, "table_%u.db", space_id);
    g_io_stats.fopen_calls++; // Count the actual open
    FILE *new_fp = fopen(filename, mode);

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
}
// --- [END OF OPTIMIZATION 2 - Step 1] ---

static long long page_offset(uint32_t page_no)
{
    return (long long)page_no * PAGE_SIZE;
}

int write_page(uint32_t space_id, uint32_t page_no, PageType type, const void *data)
{
    // --- [OPTIMIZATION 2 - Step 2: Integrate Cache into Write Path] ---
    FILE *fp = get_file_handle(space_id, "r+b");
    if (!fp)
    {
        // If r+b fails, it might be a new file. Try w+b.
        fp = get_file_handle(space_id, "w+b");
        if (!fp)
            return -1;
    }
    // --- [END OF OPTIMIZATION 2 - Step 2] ---

    if (fseek(fp, page_offset(page_no), SEEK_SET) != 0)
    {
        // We don't close the file here anymore!
        return -2;
    }

    PageHeader header = {space_id, page_no, type, 0};
    g_io_stats.fwrite_calls++;
    fwrite(&header, 1, PAGE_HEADER_SIZE, fp);
    fwrite(data, 1, PAGE_DATA_SIZE, fp);

    // NO fclose(fp) HERE! The cache manages it.
    return 0;
}

int read_page(uint32_t space_id, uint32_t page_no, void *buf, PageHeader *out_header)
{
    // --- [OPTIMIZATION 2 - Step 3: Integrate Cache into Read Path] ---
    FILE *fp = get_file_handle(space_id, "rb");
    if (!fp)
    {
        return -1;
    }
    // --- [END OF OPTIMIZATION 2 - Step 3] ---

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

    // NO fclose(fp) HERE!
    return 0;
}