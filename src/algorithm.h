#ifndef _ALGORITHM_HEADER
#define _ALGORITHM_HEADER

/*
    To get neighborint pages:
    Look at pages in the future
    Calculate distance metric
        time difference (cycle)
        virtual address
        instruction pointer
    If empty spots in neighboring pages then put them in
    If no empty spots compare distance and replace if smaller with furthest distance

    Normalization of distance metric:
        running mean and standard devation?
        max and min? (0-1 norm)

*/

#include "tmem.h"

#ifndef DEC_MIG_TIME
    #define DEC_MIG_TIME 0.01
#endif

#ifndef HISTORY_SIZE
    #define HISTORY_SIZE 16
#endif

#ifndef MAX_PRED_DEPTH
    #define MAX_PRED_DEPTH 16
#endif


extern struct tmem_page *page_history[HISTORY_SIZE];
extern uint32_t page_his_idx;
extern double mig_time;
extern double mig_queue_time;
extern double mig_move_time;
extern double bot_dist;
extern double avg_dist;

void algo_add_page(struct tmem_page *page);
struct tmem_page* algo_predict_page(struct tmem_page *page);
void algo_predict_pages(struct tmem_page *page, struct tmem_page **pred_pages, uint32_t *idx);

#endif