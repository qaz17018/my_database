#include <stdio.h>
#include <time.h>
#include "buffer_pool.h"
#include "b_tree.h"
#include "log_manager.h"
#include "io_stats.h"

void file_cache_init();
void file_cache_flush_all();

int main()
{
    printf("========================================================\n");
    printf("||       Benchmark: Write-Ahead Log Performance       ||\n");
    printf("========================================================\n\n");

    const int NUM_INSERTS = 3000000; // Let's do 20,000 inserts

    // --- Phase 1: Performance WITHOUT WAL benefit (current state) ---
    // In this phase, we write to the log AND still flush all dirty pages.
    printf("--- Phase 1: Testing performance WITH redundant page flushing ---\n");

    remove("table_perf.db");
    remove("perf_test.log");

    buf_init();
    file_cache_init();
    log_manager_init("perf_test.log");
    io_stats_init();

    clock_t start_time = clock();

    for (int i = 0; i < NUM_INSERTS; i++)
    {
        Row r = {.id = i};
        sprintf(r.username, "user%d", i);
        table_insert_row(1, &r);
    }

    // This simulates the old, slow way: flushing everything
    buf_flush_all();
    log_manager_flush();

    clock_t end_time = clock();
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("Phase 1 finished in %.4f seconds.\n", time_spent);
    io_stats_print();

    // --- Phase 2: Performance WITH WAL benefit ---
    // In this phase, we ONLY flush the log, simulating a commit.
    // We do NOT flush the dirty data pages, relying on the log for durability.
    printf("\n--- Phase 2: Testing performance WITH WAL optimization (log flush only) ---\n");

    remove("table_perf.db");
    remove("perf_test.log");

    buf_init();
    file_cache_init();
    log_manager_init("perf_test.log");
    io_stats_init();

    start_time = clock();

    for (int i = 0; i < NUM_INSERTS; i++)
    {
        Row r = {.id = i};
        sprintf(r.username, "user%d", i);
        table_insert_row(1, &r);
    }

    // This is the FAST way: only the log is guaranteed to be on disk.
    log_manager_flush();

    end_time = clock();
    time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("Phase 2 finished in %.4f seconds.\n", time_spent);
    io_stats_print();

    log_manager_shutdown();
    file_cache_flush_all();

    return 0;
}