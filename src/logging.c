#include "logging.h"

FILE* debug_fp = NULL;
FILE* stats_fp = NULL;
FILE* time_fp = NULL;
FILE* pred_fp = NULL;
FILE* mig_fp = NULL;
FILE* cold_fp = NULL;
struct timespec log_start_time;

void init_log_files() {
    internal_call = true;
    
    debug_fp = fopen("debuglog.txt", "w");
    assert(debug_fp != NULL);

    stats_fp = fopen("stats.txt", "w");
    assert(stats_fp != NULL);

    time_fp = fopen("time.txt", "w");
    assert(time_fp != NULL);

    pred_fp = fopen("preds.bin", "wb");
    assert(pred_fp != NULL);

    mig_fp = fopen("mig.bin", "wb");
    assert(mig_fp != NULL);

    cold_fp = fopen("cold.bin", "wb");
    assert(cold_fp != NULL);

    internal_call = false;

    // reference start time
    log_start_time = get_time();
}