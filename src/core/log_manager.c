#include "log_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_BUFFER_SIZE (1024 * 1024) // 1MB 的日志缓冲区

static struct
{
    FILE *log_file;
    char *buffer;
    uint32_t current_pos; // 当前在缓冲区中的位置
    uint64_t current_lsn; // 全局递增的 LSN
} g_log_manager;

int log_manager_init(const char *log_file_path)
{
    // 'a+b' 模式：以二进制追加模式打开，如果文件不存在则创建
    g_log_manager.log_file = fopen(log_file_path, "a+b");
    if (!g_log_manager.log_file)
    {
        perror("Failed to open log file");
        return -1;
    }

    g_log_manager.buffer = (char *)malloc(LOG_BUFFER_SIZE);
    if (!g_log_manager.buffer)
    {
        fclose(g_log_manager.log_file);
        return -1;
    }

    g_log_manager.current_pos = 0;

    // 在真实数据库中，需要从日志文件中恢复 LSN，这里我们简化
    g_log_manager.current_lsn = 1;

    printf("Log Manager initialized.\n");
    return 0;
}

void log_manager_flush()
{
    if (g_log_manager.current_pos > 0)
    {
        fwrite(g_log_manager.buffer, 1, g_log_manager.current_pos, g_log_manager.log_file);
        fflush(g_log_manager.log_file); // 确保数据真正写入磁盘
        g_log_manager.current_pos = 0;
    }
}

void log_manager_shutdown()
{
    log_manager_flush();
    fclose(g_log_manager.log_file);
    free(g_log_manager.buffer);
    printf("Log Manager shut down.\n");
}

uint64_t log_append_update_record(uint32_t space_id, uint32_t page_no, uint16_t offset, uint16_t len, const void *data)
{
    uint32_t record_size = sizeof(LogRecordHeader) + len;

    // 如果缓冲区剩余空间不足，则先刷盘
    if (g_log_manager.current_pos + record_size > LOG_BUFFER_SIZE)
    {
        log_manager_flush();
    }

    // 构造日志头
    LogRecordHeader header;
    header.lsn = g_log_manager.current_lsn++;
    header.type = LOG_RECORD_TYPE_UPDATE;
    header.space_id = space_id;
    header.page_no = page_no;
    header.offset = offset;
    header.len = len;

    // 拷贝日志头和数据到缓冲区
    memcpy(g_log_manager.buffer + g_log_manager.current_pos, &header, sizeof(LogRecordHeader));
    g_log_manager.current_pos += sizeof(LogRecordHeader);
    memcpy(g_log_manager.buffer + g_log_manager.current_pos, data, len);
    g_log_manager.current_pos += len;

    return header.lsn;
}