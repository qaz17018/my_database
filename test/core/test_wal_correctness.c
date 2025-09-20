#include <stdio.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "log_manager.h"
#include "log_utils.h" // 引入我们的日志打印工具

void file_cache_init();
void file_cache_flush_all();

int main()
{
    printf("========================================================\n");
    printf("||         Test: Write-Ahead Log Correctness          ||\n");
    printf("========================================================\n\n");

    const char *LOG_FILE = "correctness_test.log";
    const uint32_t SPACE_ID = 1;

    // 清理旧文件
    remove(LOG_FILE);
    remove("table_1.db");

    buf_init();
    file_cache_init();
    log_manager_init(LOG_FILE);

    // --- 插入几条记录 ---
    printf("--- Phase 1: Inserting 3 rows into table %u ---\n", SPACE_ID);
    for (int i = 0; i < 3; i++)
    {
        Row r = {.id = i};
        sprintf(r.username, "user%d", i);
        // 我们需要确保调用的是我们已经改造过的 leaf_page_insert
        // table_insert_row 会触发它
        if (table_insert_row(SPACE_ID, &r) != 0)
        {
            fprintf(stderr, "Insertion failed for row %d\n", i);
        }
    }
    printf("Insertion complete. Shutting down to flush log...\n");

    // 关闭数据库，这将强制日志缓冲区被刷入磁盘
    log_manager_shutdown();
    file_cache_flush_all();

    // --- 验证日志文件 ---
    printf("\n--- Phase 2: Verifying contents of log file ---\n");
    print_log_file(LOG_FILE);

    printf("Test complete. Please check the log output above.\n");

    return 0;
}