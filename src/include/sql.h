#ifndef SQL_H
#define SQL_H

#include "row.h"

typedef enum
{
    STATEMENT_INSERT,
    STATEMENT_SELECT,
    STATEMENT_DELETE
} StatementType;

// [新增] 定义 WHERE 子句的类型
typedef enum
{
    WHERE_BY_ID,
    WHERE_BY_USERNAME
} WhereClauseType;

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
            WhereClauseType where_type; // 指明是按ID还是按username查
            union
            {
                uint32_t id;
                char username[USERNAME_MAX_LEN];
            } where_params;
        } select_statement;

        struct
        {
            uint32_t id_to_delete;
        } delete_statement;

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