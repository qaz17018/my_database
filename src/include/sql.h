#ifndef SQL_H
#define SQL_H

#include "row.h"

typedef enum
{
    STATEMENT_INSERT,
    STATEMENT_SELECT,
    STATEMENT_DELETE,
    STATEMENT_UPDATE
} StatementType;

// [新增] 定义 WHERE 子句的类型
typedef enum
{
    WHERE_BY_ID,
    WHERE_BY_USERNAME,
    WHERE_BY_ID_RANGE // [新增] 代表ID范围查询
} WhereClauseType;

// [新增] 定义一个枚举来代表所有可能的列
typedef enum
{
    COLUMN_ID,
    COLUMN_USERNAME,
    COLUMN_EMAIL
} ColumnType;

typedef struct
{
    StatementType type;
    union
    {
        struct
        {
            Row row_to_insert;
        } insert_statement;

        // [核心修改] 升级 select 指令，让它能携带更复杂的查询条件
        struct
        {
            // [核心修改] 为SELECT指令添加投影列表
            ColumnType columns_to_select[3]; // 最多选择3列
            int num_columns_to_select;       // 实际选择的列数

            WhereClauseType where_type; // 指明是按ID还是按username查
            union
            {
                uint32_t id;
                char username[USERNAME_MAX_LEN];

                // [新增] 为范围查询定义一个新的结构
                struct
                {
                    uint32_t lower_bound; // ID下限
                    uint32_t upper_bound; // ID上限
                } id_range;
            } where_params;
        } select_statement;

        struct
        {
            uint32_t id_to_delete;
        } delete_statement;

        struct
        {
            uint32_t id_to_update;
            Row new_row_data; // 用一个 Row 结构来携带所有新数据
        } update_statement;

    } params;
} Statement;

// 定义前端“解读”过程可能产生的结果
typedef enum
{
    PREPARE_SUCCESS,                // 解读成功
    PREPARE_UNRECOGNIZED_STATEMENT, // 无法识别的命令 (比如 "update")
    PREPARE_SYNTAX_ERROR            // 语法错误 (比如 "insert" 写错了)
} PrepareResult;

// --- 函数声明 ---

// “前端”主函数：负责解读SQL字符串，并填写 Statement 结构体
PrepareResult prepare_statement(char *input, Statement *statement);

// “后端”主函数：负责执行一个已经被解读好的 Statement
void execute_statement(uint32_t space_id, Statement *statement);

#endif