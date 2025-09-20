#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/sql.h"
#include "include/buffer_pool.h" // 注意这里的路径
#include "include/log_manager.h" // <--- [新增]

// 打印命令行提示符
void print_prompt()
{
    printf("my_db > ");
}

int main(int argc, char *argv[])
{
    // 初始化存储引擎
    const uint32_t DB_SPACE_ID = 1;
    buf_init();
    if (log_manager_init("my_db.log") != 0)
    { // <--- [新增]
        fprintf(stderr, "Failed to initialize log manager.\n");
        return 1;
    }

    char input_buffer[256];

    while (1)
    {
        print_prompt();

        // 读取用户输入的完整一行
        if (!fgets(input_buffer, sizeof(input_buffer), stdin))
        {
            printf("\nBye!\n");
            break; // 用户按下 Ctrl+D
        }

        // 移除末尾的换行符
        input_buffer[strcspn(input_buffer, "\n")] = 0;

        // 如果用户输入 ".exit"
        if (strcmp(input_buffer, ".exit") == 0)
        {
            printf("Bye!\n");
            buf_flush_all();        // 退出前确保所有数据都已保存
            log_manager_shutdown(); // <--- [新增]
            break;
        }

        // --- SQL处理流程 ---
        Statement statement;
        switch (prepare_statement(input_buffer, &statement))
        {
        case PREPARE_SUCCESS:
            execute_statement(DB_SPACE_ID, &statement);
            break;
        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error near '%s'.\n", input_buffer);
            break;
        case PREPARE_UNRECOGNIZED_STATEMENT:
            printf("Unrecognized command: '%s'.\n", input_buffer);
            break;
        }
    }

    return 0;
}