#ifndef IO_STATS_H
#define IO_STATS_H

#include <stdint.h>

// 定义一个全局的统计结构体
typedef struct
{
    uint64_t fopen_calls;
    uint64_t fclose_calls;
    uint64_t fwrite_calls;
    uint64_t fread_calls;
} IOStats;

// 声明一个外部变量，以便在任何地方都能访问
extern IOStats g_io_stats;

// 声明初始化和打印函数
void io_stats_init();
void io_stats_print();

#endif