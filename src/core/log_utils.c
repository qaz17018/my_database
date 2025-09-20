#include "log_utils.h"
#include "log_manager.h" // 需要 LogRecordHeader 的定义
#include <stdio.h>

void print_log_file(const char *log_file_path)
{
    FILE *fp = fopen(log_file_path, "rb");
    if (!fp)
    {
        printf("Could not open log file: %s\n", log_file_path);
        return;
    }

    printf("\n--- Parsing Log File: %s ---\n", log_file_path);
    LogRecordHeader header;
    int record_count = 0;

    // 循环读取日志头
    while (fread(&header, sizeof(LogRecordHeader), 1, fp) == 1)
    {
        record_count++;
        printf("  [Record #%d | LSN: %llu | Type: %d | Page: %u:%u | Offset: %u | Len: %u]\n",
               record_count, header.lsn, header.type, header.space_id, header.page_no, header.offset, header.len);

        // 根据头中的长度，跳过数据部分
        if (fseek(fp, header.len, SEEK_CUR) != 0)
        {
            printf("    Error: Failed to seek past data for LSN %llu\n", header.lsn);
            break;
        }
    }

    if (record_count == 0)
    {
        printf("  Log file is empty or does not contain valid records.\n");
    }
    printf("--- End of Log File ---\n\n");
    fclose(fp);
}