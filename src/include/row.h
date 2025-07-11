#ifndef ROW_H
#define ROW_H

#include <stdint.h>

#define MAX_NAME_LEN 32
#define MAX_ROWS_PER_PAGE 256

#define USERNAME_MAX_LEN 32
#define EMAIL_MAX_LEN 255

typedef struct
{
    uint32_t id;
    char username[USERNAME_MAX_LEN];
    char email[EMAIL_MAX_LEN];
} Row;

typedef struct
{
    uint16_t num_records;
    Row records[MAX_ROWS_PER_PAGE];
} RowPage;

int row_page_insert(RowPage *page, const Row *row);

Row *row_page_find(RowPage *page, uint32_t key);

#endif