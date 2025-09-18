#include "io_stats.h"
#include <stdio.h>
#include <string.h>

// 定义全局变量
IOStats g_io_stats;

void io_stats_init()
{
    memset(&g_io_stats, 0, sizeof(IOStats));
}

void io_stats_print()
{
    printf("\n--- I/O Statistics ---\n");
    printf("  fopen calls : %llu\n", g_io_stats.fopen_calls);
    printf("  fclose calls: %llu\n", g_io_stats.fclose_calls);
    printf("  fwrite calls: %llu\n", g_io_stats.fwrite_calls);
    printf("  fread calls : %llu\n", g_io_stats.fread_calls);
    printf("----------------------\n\n");
}