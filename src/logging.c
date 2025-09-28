#include "logging.h"

FILE* debug_fp = NULL;
FILE* stats_fp = NULL;

void init_log_files() {
    debug_fp = fopen("debuglog.txt", "w");
    assert(debug_fp != NULL);

    stats_fp = fopen("stats.txt", "w");
    assert(stats_fp != NULL);
}