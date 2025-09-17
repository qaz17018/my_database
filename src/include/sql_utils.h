#ifndef SQL_UTILS_H
#define SQL_UTILS_H
#include "sql.h" // 需要 Statement 和 Row 的定义
void print_projected_row(const Row *row, const Statement *statement);
#endif