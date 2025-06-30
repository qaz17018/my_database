#include "page.h"
#include <stdio.h>
#include <string.h>

#define DB_FILE "mydb.data"

static long long page_offset(uint32_t space_id, uint32_t page_no)
{
    return ((long long)space_id * MAX_PAGES_PER_SPACE + page_no) * PAGE_SIZE;
}

int write_page(uint32_t space_id, uint32_t page_no, PageType type, const void *data)
{
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
    fwrite(&header, 1, PAGE_HEADER_SIZE, fp);
    fwrite(data, 1, PAGE_DATA_SIZE, fp);
    fclose(fp);
    return 0;
}

int read_page(uint32_t space_id, uint32_t page_no, void *buf, PageHeader *out_header)
{
    FILE *fp = fopen(DB_FILE, "rb");
    if (!fp)
        return -1;

    if (fseek(fp, page_offset(space_id, page_no), SEEK_SET) != 0)
    {
        fclose(fp);
        return -2;
    }

    fread(out_header, 1, PAGE_HEADER_SIZE, fp);
    fread(buf, 1, PAGE_DATA_SIZE, fp);
    fclose(fp);
    return 0;
}