#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stdint.h>
#include "page.h" // 需要 PageHeader 的定义

// --- 日志记录类型 ---
// 目前我们只关心一种：对数据页的更新
typedef enum
{
    LOG_RECORD_TYPE_UPDATE,
    // 未来可以扩展，例如 LOG_RECORD_TYPE_COMMIT 等
} LogRecordType;

// --- 日志记录结构体 ---
// 描述了一次对页面的写入操作
typedef struct
{
    uint64_t lsn;       // 日志序列号 (Log Sequence Number)，唯一且递增
    LogRecordType type; // 日志类型
    uint32_t space_id;  // 哪个表空间
    uint32_t page_no;   // 哪个页
    uint16_t offset;    // 页内的哪个偏移量
    uint16_t len;       // 写入数据的长度
    // data 字段将紧跟在这个结构体后面
} LogRecordHeader;

// --- 日志管理器接口 ---

/**
 * @brief 初始化日志管理器
 * @param log_file_path 日志文件的路径
 * @return 0 表示成功, -1 表示失败
 */
int log_manager_init(const char *log_file_path);

/**
 * @brief 关闭日志管理器，确保所有缓冲区的日志都已刷盘
 */
void log_manager_shutdown();

/**
 * @brief 将一条“更新”操作写入到日志缓冲区
 * @param space_id 表空间ID
 * @param page_no 页号
 * @param offset 写入的偏移量
 * @param len 写入的数据长度
 * @param data 指向要写入数据的指针
 * @return 返回这条日志的 LSN
 */
uint64_t log_append_update_record(uint32_t space_id, uint32_t page_no, uint16_t offset, uint16_t len, const void *data);

/**
 * @brief 强制将日志缓冲区的内容刷入磁盘
 */
void log_manager_flush();

#endif