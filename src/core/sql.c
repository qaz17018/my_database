#include <stdio.h>
#include <string.h>
#include "sql.h"
#include "b_tree.h"
#include "secondary_b_tree.h"

static PrepareResult prepare_select(char *input, Statement *statement)
{
    statement->type = STATEMENT_SELECT;

    // 我们使用 strstr 来查找 "where" 子句的类型，这比 sscanf 更灵活
    char *where_clause = strstr(input, "where");
    if (!where_clause)
    {
        return PREPARE_SYNTAX_ERROR; // 必须有 where 子句
    }

    // Case 1: 检查是否是按 id 查询
    if (strstr(where_clause, "id ="))
    {
        statement->params.select_statement.where_type = WHERE_BY_ID;
        int args_assigned = sscanf(where_clause, "where id = %d",
                                   &statement->params.select_statement.where_params.id);
        if (args_assigned != 1)
        {
            return PREPARE_SYNTAX_ERROR;
        }
        return PREPARE_SUCCESS;
    }

    // Case 2: 检查是否是按 username 查询
    if (strstr(where_clause, "username ="))
    {
        statement->params.select_statement.where_type = WHERE_BY_USERNAME;
        int args_assigned = sscanf(where_clause, "where username = '%[^']'",
                                   statement->params.select_statement.where_params.username);
        if (args_assigned != 1)
        {
            return PREPARE_SYNTAX_ERROR;
        }
        return PREPARE_SUCCESS;
    }

    // 如果两种都不是，就是语法错误
    return PREPARE_SYNTAX_ERROR;
}

// 一个专门解析 INSERT 语句的内部辅助函数
static PrepareResult prepare_insert(char *input, Statement *statement)
{
    statement->type = STATEMENT_INSERT;

    // strtok 是一个简单的“分词器”，它会按空格分割字符串
    char *keyword_insert = strtok(input, " ");
    char *keyword_into = strtok(NULL, " ");
    char *table_name = strtok(NULL, " "); // 我们暂时不使用 table_name
    char *keyword_values = strtok(NULL, " ");

    // 进行简单的语法检查
    if (!keyword_insert || !keyword_into || !table_name || !keyword_values ||
        strcmp(keyword_insert, "insert") != 0 ||
        strcmp(keyword_into, "into") != 0 ||
        strcmp(keyword_values, "values") != 0)
    {
        return PREPARE_SYNTAX_ERROR;
    }

    // sscanf 是一个强大的格式化读取函数，它可以从字符串中提取数据
    // 我们用它来解析 "(id, 'username', 'email')" 这种格式
    // %[^'] 的意思是：读取所有不是单引号(')的字符
    int args_assigned = sscanf(strtok(NULL, ""), "(%d, '%[^']', '%[^']')",
                               &statement->params.insert_statement.row_to_insert.id,
                               statement->params.insert_statement.row_to_insert.username,
                               statement->params.insert_statement.row_to_insert.email);

    // 必须成功解析出3个参数，否则就是语法错误
    if (args_assigned != 3)
    {
        return PREPARE_SYNTAX_ERROR;
    }

    return PREPARE_SUCCESS;
}

static PrepareResult prepare_delete(char *input, Statement *statement)
{
    statement->type = STATEMENT_DELETE;

    // 使用 sscanf 解析 "delete from users where id = %d"
    int args_assigned = sscanf(input, "delete from users where id = %d",
                               &statement->params.delete_statement.id_to_delete);

    if (args_assigned != 1)
    {
        return PREPARE_SYNTAX_ERROR;
    }
    return PREPARE_SUCCESS;
}

// “前端”主函数：prepare_statement
PrepareResult prepare_statement(char *input, Statement *statement)
{
    // 我们目前只支持以 "insert" 开头的命令
    if (strncmp(input, "insert", 6) == 0)
    {
        return prepare_insert(input, statement);
    }

    // 将来我们会在这里添加对 "select" 的支持
    if (strncmp(input, "select", 6) == 0)
    {
        return prepare_select(input, statement);
    }

    if (strncmp(input, "delete", 6) == 0)
    {
        return prepare_delete(input, statement);
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

Row *b_tree_search(uint32_t space_id, uint32_t id);

// 一个辅助函数，用于打印查询到的行
static void print_row(const Row *row)
{
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

// “后端”主函数：execute_statement
void execute_statement(uint32_t space_id, Statement *statement)
{
    switch (statement->type)
    {
    case STATEMENT_INSERT:
        // 收到“插入”指令，调用我们早已写好的 table_insert_row 函数
        if (table_insert_row(space_id, &(statement->params.insert_statement.row_to_insert)) == 0)
        {
            printf("Executed.\n");
        }
        else
        {
            printf("Error: Failed to execute insert statement.\n");
        }
        break;
    case STATEMENT_SELECT:
    {
        // [核心修改] 根据 where_type 执行不同的查询路径
        Row *row_found = NULL;
        if (statement->params.select_statement.where_type == WHERE_BY_ID)
        {
            // 路径1：按ID查询
            uint32_t id = statement->params.select_statement.where_params.id;
            printf("Executing SELECT by id = %d\n", id);
            row_found = b_tree_search(space_id, id);
        }
        else if (statement->params.select_statement.where_type == WHERE_BY_USERNAME)
        {
            // 路径2：按用户名查询
            char *username = statement->params.select_statement.where_params.username;
            printf("Executing SELECT by username = '%s'\n", username);

            // 第1步：通过辅助索引，查找主键ID
            uint32_t pk_found = table_search_pk_by_username(space_id, username);

            if (pk_found > 0)
            {
                // 第2步：回表！拿着找到的主键ID，去主B+树查找完整的行
                row_found = b_tree_search(space_id, pk_found);
            }
        }

        // 统一处理查询结果
        if (row_found)
        {
            print_row(row_found);
        }
        else
        {
            printf("Row not found.\n");
        }
    }
    break;

        // [新增] 实现 DELETE 的执行逻辑
    case STATEMENT_DELETE:
    {
        uint32_t id = statement->params.delete_statement.id_to_delete;
        printf("Executing DELETE for id %d\n", id);

        // 我们将调用一个新的顶层删除函数
        if (table_delete_row(space_id, id) == 0)
        {
            printf("Executed.\n");
        }
        else
        {
            printf("Error: Failed to execute delete statement (row with id %d not found?).\n", id);
        }
    }
    break;
    }
}