#include "page.h"
#include <stdio.h>
#include <string.h>
#include "io_stats.h" // <--- [新增] 包含我们的新头文件

#define DB_FILE "mydb.data"

static long long page_offset(uint32_t space_id, uint32_t page_no)
{
    return ((long long)space_id * MAX_PAGES_PER_SPACE + page_no) * PAGE_SIZE;
}

int write_page(uint32_t space_id, uint32_t page_no, PageType type, const void *data)
{
    g_io_stats.fopen_calls++; // <--- [新增] 计数 fopen
    FILE *fp = fopen(DB_FILE, "r+b");
    if (!fp)
    {
        fp = fopen(DB_FILE, "w+b");
        if (!fp)
            return -1;
    }

    if (fseek(fp, page_offset(space_id, page_no), SEEK_SET) != 0)
    {
        fclose(fp);
        return -2;
    }

    PageHeader header = {space_id, page_no, type, 0};
    g_io_stats.fwrite_calls++; // <--- [新增] 计数 fwrite
    fwrite(&header, 1, PAGE_HEADER_SIZE, fp);
    fwrite(data, 1, PAGE_DATA_SIZE, fp);

    g_io_stats.fclose_calls++; // <--- [新增] 计数 fclose
    fclose(fp);
    return 0;
}

int read_page(uint32_t space_id, uint32_t page_no, void *buf, PageHeader *out_header)
{
    g_io_stats.fopen_calls++;
    FILE *fp = fopen(DB_FILE, "rb");
    if (!fp)
        return -1;

    if (fseek(fp, page_offset(space_id, page_no), SEEK_SET) != 0)
    {
        fclose(fp);
        return -2;
    }

    g_io_stats.fread_calls++;
    // --- [THE FIX] ---
    // Verify that we actually read the correct number of bytes for the header.
    // If fread returns a value less than requested, it means we hit the end of the file or an error.
    if (fread(out_header, 1, PAGE_HEADER_SIZE, fp) != PAGE_HEADER_SIZE)
    {
        fclose(fp);
        return -3; // Return a new error code for "read failed"
    }
    // Do the same verification for the page data.
    if (fread(buf, 1, PAGE_DATA_SIZE, fp) != PAGE_DATA_SIZE)
    {
        fclose(fp);
        return -3; // Return the same error code
    }
    // --- [END OF FIX] ---

    g_io_stats.fclose_calls++;
    fclose(fp);
    return 0;
}