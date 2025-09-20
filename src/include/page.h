#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>

#define PAGE_SIZE 16384
#define MAX_PAGES_PER_SPACE 1000

typedef enum
{
    PAGE_TYPE_META = 0,
    PAGE_TYPE_LEAF = 1,               // 主B+树的叶子
    PAGE_TYPE_INTERNAL = 2,           // 主B+树的内节点
    PAGE_TYPE_SECONDARY_LEAF = 3,     // [新增] 辅助B+树的叶子
    PAGE_TYPE_SECONDARY_INTERNAL = 4, // [新增] 辅助B+树的内节点
    PAGE_TYPE_FREE = 5
} PageType;

typedef struct
{
    uint32_t space_id;
    uint32_t page_no;
    uint16_t page_type;
    uint16_t reserved;
    // 记录修改此页的最后一条日志的 LSN
    uint64_t page_lsn;
} PageHeader;

#define PAGE_HEADER_SIZE sizeof(PageHeader)
#define PAGE_DATA_SIZE (PAGE_SIZE - PAGE_HEADER_SIZE)

int write_page(uint32_t space_id, uint32_t page_no, PageType type, const void *data, uint64_t lsn);
int read_page(uint32_t space_id, uint32_t page_no, void *buf, PageHeader *out_header);

#endif