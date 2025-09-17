#include "sql_utils.h"
#include <stdio.h>
// 将之前 sql.c 中的 print_row 函数完整地搬到这里
void print_projected_row(const Row *row, const Statement *statement)
{
    printf("(");
    for (int i = 0; i < statement->params.select_statement.num_columns_to_select; i++)
    {
        switch (statement->params.select_statement.columns_to_select[i])
        {
        case COLUMN_ID:
            printf("%d", row->id);
            break;
        case COLUMN_USERNAME:
            printf("'%s'", row->username);
            break;
        case COLUMN_EMAIL:
            printf("'%s'", row->email);
            break;
        }
        if (i < statement->params.select_statement.num_columns_to_select - 1)
        {
            printf(", ");
        }
    }
    printf(")\n");
}