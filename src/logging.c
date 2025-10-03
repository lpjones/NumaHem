#include "logging.h"

FILE* debug_fp = NULL;
FILE* stats_fp = NULL;

void init_log_files() {
    internal_call = true;
    debug_fp = fopen("debuglog.txt", "w");
    assert(debug_fp != NULL);

    stats_fp = fopen("stats.txt", "w");
    assert(stats_fp != NULL);
    internal_call = false;
}