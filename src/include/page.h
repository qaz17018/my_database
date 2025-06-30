#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>

#define PAGE_SIZE 16384
#define MAX_PAGES_PER_SPACE 1000

typedef enum
{
    PAGE_TYPE_LEAF = 1,     // [修改] 为了更清晰，将DATA改为LEAF
    PAGE_TYPE_INTERNAL = 2, // [新增] 内节点类型
    PAGE_TYPE_FREE = 3
} PageType;

typedef struct
{
    uint32_t space_id;
    uint32_t page_no;
    uint16_t page_type;
    uint16_t reserved;
} PageHeader;

#define PAGE_HEADER_SIZE sizeof(PageHeader)
#define PAGE_DATA_SIZE (PAGE_SIZE - PAGE_HEADER_SIZE)

int write_page(uint32_t space_id, uint32_t page_no, PageType type, const void *data);
int read_page(uint32_t space_id, uint32_t page_no, void *buf, PageHeader *out_header);

#endif