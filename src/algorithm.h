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

#define HISTORY_SIZE 16

extern struct tmem_page *page_history[HISTORY_SIZE];
extern uint32_t page_his_idx;

void algo_add_page(struct tmem_page *page);
struct tmem_page* algo_predict_page(struct tmem_page *page);

#endif