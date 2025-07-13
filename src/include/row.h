#ifndef ROW_H
#define ROW_H

#include <stdint.h>

#define USERNAME_MAX_LEN 32
#define EMAIL_MAX_LEN 255

typedef struct
{
    uint32_t id;
    char username[USERNAME_MAX_LEN];
    char email[EMAIL_MAX_LEN];
} Row;

// [清理] 下面的定义已经不再使用，并且存在缓冲区溢出风险，将其移除。
/*
#define MAX_ROWS_PER_PAGE 256
typedef struct
{
    uint16_t num_records;
    Row records[MAX_ROWS_PER_PAGE];
} RowPage;

int row_page_insert(RowPage *page, const Row *row);
Row *row_page_find(RowPage *page, uint32_t key);
*/

#endif