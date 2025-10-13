#ifndef _LOGGING_HEADER
#define _LOGGING_HEADER

#include <stdio.h>
#include <assert.h>

#include "tmem.h"

extern FILE* stats_fp;
extern FILE* debug_fp;
extern FILE* time_fp;
extern struct timespec log_start_time;

// #define LOG_DEBUG(...) { fprintf(debug_fp, __VA_ARGS__); fflush(debug_fp); }
#define LOG_DEBUG(...) {                                                     \
    fprintf(debug_fp, "[%.9f]\t", elapsed_time(log_start_time, get_time())); \
    fprintf(debug_fp, __VA_ARGS__);                                          \
    fflush(debug_fp);                                                        \
}
#define LOG_STATS(...) { fprintf(stats_fp, __VA_ARGS__); fflush(stats_fp); }
#define LOG_TIME(...) { fprintf(time_fp, __VA_ARGS__); fflush(time_fp); }

void init_log_files();


#endif