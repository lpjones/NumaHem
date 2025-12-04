#include "algorithm.h"
#include <math.h>

#define ABS(x) ((x) >= 0 ? (x) : -(x))

#ifndef VA_WEIGHT
#define VA_WEIGHT 1
#endif

#ifndef CYC_WEIGHT
#define CYC_WEIGHT 1
#endif

#ifndef IP_WEIGHT
#define IP_WEIGHT 1
#endif

#ifndef DEC_UP
#define DEC_UP 0.01
#endif

#ifndef DEC_DOWN
#define DEC_DOWN 0.0001
#endif

#ifndef DEC_DIST
#define DEC_DIST 0.0001
#endif



struct tmem_page *page_history[HISTORY_SIZE];
uint32_t page_his_idx = 0;
double mig_time = 0;
double mig_queue_time = 0;
double mig_move_time = 0;

double avg_dist = 1;
double bot_dist = 1;


// static double top_va = 2, bot_va = 1;
// static double top_cyc = 2, bot_cyc = 1;
// static double top_ip = 2, bot_ip = 1;

// // Trends towards upper part of range but still less than max
// static double update_top(double top, double val) {
//     if (val > top) {
//         return DEC_UP * val + (1.0 - DEC_UP) * top;
//     }
//     return DEC_DOWN * val + (1.0 - DEC_DOWN) * top;
// }

// Trends towards lower part of range but still greater than min
static inline double update_bot(double bot, double val) {
    if (val < bot / 10) {
        val = bot / 10;
    }
    if (val < bot) {
        return DEC_UP * val + (1.0 - DEC_UP) * bot;
    }
    if (val > bot * 10) {
        val = bot * 10;
    }
    // val = sqrt(val - bot) + bot;
    return DEC_DOWN * val + (1.0 - DEC_DOWN) * bot;
}

static double calc_distance(struct tmem_page *a, struct tmem_page *b) {
    double distance = 0;
    // double x = 5;
    // printf("%f -> %f\n", (double)(a->va) - (double)(b->va), ABS((double)(a->va) - (double)(b->va)));
    double va_diff = ABS((double)(a->va) - (double)(b->va));
    double cyc_diff = ABS((double)(a->cyc_accessed) - (double)(b->cyc_accessed));
    double ip_diff = ABS((double)(a->ip) - (double)(b->ip));

    // update ranges
    // top_va = update_top(top_va, va_diff);
    // top_cyc = update_top(top_cyc, cyc_diff);
    // top_ip = update_top(top_ip, ip_diff);

    // bot_va = update_bot(bot_va, va_diff);
    // bot_cyc = update_bot(bot_cyc, cyc_diff);
    // bot_ip = update_bot(bot_ip, ip_diff);

    // va_diff = (va_diff - bot_va) / (top_va - bot_va);
    // cyc_diff = (cyc_diff - bot_cyc) / (top_cyc - bot_cyc);
    // ip_diff = (ip_diff - bot_ip) / (top_ip - bot_ip);

    // printf("va: %f, cyc: %f, ip: %f\n", va_diff, cyc_diff, ip_diff);


    distance += va_diff * VA_WEIGHT;
    distance += cyc_diff * CYC_WEIGHT;
    distance += ip_diff * IP_WEIGHT;

    // if (distance == 0) return ;

    double percent_dram = pebs_stats.dram_accesses / (pebs_stats.dram_accesses + pebs_stats.rem_accesses + 1);

    bot_dist = update_bot(bot_dist, distance * (1 - percent_dram * percent_dram));

    // when the percent is good you want it to do less (lower threshold)
    // when the percent is bad you want it to do more (higher threshold)


    avg_dist = DEC_DIST * distance + (1.0 - DEC_DIST) * avg_dist;
    // dist_count++;
    // printf("avg_dist: %f\n", avg_dist);

    return distance;
}

static void update_neighbors(struct tmem_page *old_page) {
    // cool neighbors
    for (uint32_t i = 0; i < MAX_NEIGHBORS; i++) {
        old_page->neighbors[i].distance *= 1.01;
    }

    for (uint32_t i = 0; i < HISTORY_SIZE; i++) {
        struct tmem_page *cur_page = page_history[i];
        if (cur_page == old_page) continue;

        double distance = calc_distance(old_page, cur_page);
        // assert(distance != 0);
        
        // Find empty spot or furthest distance neighbor O(MAX_NEIGHBORS)
        struct neighbor_page *furthest_neighbor = NULL;
        for (uint32_t j = 0; j < MAX_NEIGHBORS; j++) {
            if (old_page->neighbors[j].page == cur_page) {
                // already a neighbor, update and continue
                // LOG_DEBUG("Already a neighbor\n");
                furthest_neighbor = &old_page->neighbors[j];
                furthest_neighbor->distance = 0;
                break;
            }
            if (old_page->neighbors[j].page == NULL) {  // empty spot
                // LOG_DEBUG("Empty spot\n");
                assert(old_page->neighbors[j].distance == 0);
                assert(old_page->neighbors[j].time_diff == 0);
                // printf("found empty spot\n");
                furthest_neighbor = &old_page->neighbors[j];
                break;
            }

            if (furthest_neighbor == NULL || old_page->neighbors[j].distance > furthest_neighbor->distance) {
                furthest_neighbor = &old_page->neighbors[j];
            }
        }

        // Replace furthest page with cur page if it's closer
        // printf("furthest: %f, distance: %f\n", furthest_neighbor->distance, distance);
        if (furthest_neighbor->distance == 0 || distance < furthest_neighbor->distance) {
            // printf("adding page\n");
            furthest_neighbor->page = cur_page;
            furthest_neighbor->distance = distance;
            furthest_neighbor->time_diff = cur_page->cyc_accessed - old_page->cyc_accessed;
        }
        
    }
    // printf("Neighbors:\t");
    // for (uint32_t i = 0; i < MAX_NEIGHBORS; i++) {
    //     if (old_page->neighbors[i].page != NULL)
    //         printf("0x%lx, ", old_page->neighbors[i].page->va);
    // }
    // printf("\n");
}

void algo_add_page(struct tmem_page *page) {
    // update neighbors of oldest page to get furthest lookahead 
    // then replace it with the new page

    // find oldest page O(HISTORY_SIZE)
    struct tmem_page *old_page = page_history[page_his_idx];
    uint32_t old_idx = page_his_idx;

    if (old_page == NULL) {
        // LOG_DEBUG("ALGO: History not full yet\n");
        // History not full yet, add page and return
        page_history[page_his_idx] = page;
        page_his_idx = (page_his_idx + 1) % HISTORY_SIZE;
        return;
    }
    for (uint32_t i = 0; i < HISTORY_SIZE; i++) {
        if (page_history[i]->cyc_accessed < old_page->cyc_accessed) {
            old_idx = i;
            old_page = page_history[i];
        }
    }

    // LOG_DEBUG("ALGO: oldest page: 0x%lx\n", old_page->va);

    update_neighbors(old_page);

    page_history[old_idx] = page;
    
}

// 29
// static void record_sample(struct tmem_page *page) {
//     struct pebs_rec p_rec = {
//         .va = page->va, //8
//         .ip = page->ip, //8
//         .cyc = page->cyc_accessed, //8
//         .cpu = 0, //4
//         .evt = page->in_dram //1
//     };
//     fwrite(&p_rec, sizeof(struct pebs_rec), 1, pred_fp);
// }

// // 45
// static void record_neighbor(struct neighbor_page *neighbor) {
//     if (neighbor->page == NULL) {
//         char buf[sizeof(struct pebs_rec)] = {0};
//         fwrite(buf, sizeof(struct pebs_rec), 1, pred_fp);
//     } else {
//         record_sample(neighbor->page);
//     }
//     // 29
//     fwrite(&neighbor->distance, sizeof(double), 1, pred_fp); //8
//     fwrite(&neighbor->time_diff, sizeof(uint64_t), 1, pred_fp); //8
// }

// Record format:
// page predicting from (pebs_record)
// neighboring pages (pebs_record + distance + time_diff)
// threshold
void algo_predict_pages(struct tmem_page *page, struct tmem_page **pred_pages, uint32_t *idx) {
    if (pebs_stats.throttles > pebs_stats.unthrottles) return;
    // record_sample(page); //29

    if (hot_list.numentries == 0) {
        mig_queue_time = 0;
    }

    assert(*idx == 0);
    // double threshold = avg_dist / 4000;
    // LOG_DEBUG("Threshold: %.2e, avg_dist: %.2e\n", bot_dist, avg_dist);
    double threshold = bot_dist;

#if DFS_ALGO == 1
    // DFS
    uint64_t tot_time_diff = 0;
    struct tmem_page *cur_page = page;
    for (uint32_t d = 0; d < MAX_PRED_DEPTH; d++) {
        struct neighbor_page *closest_neighbor = NULL;
        // if (d > 1) {
        //     LOG_DEBUG("PRED: Depth=%u\n", d);
        // }
        for (uint32_t i = 0; i < MAX_NEIGHBORS; i++) {
            if (cur_page->neighbors[i].distance != 0 && cur_page->neighbors[i].distance < threshold) {
                // found close neighbor
                if (closest_neighbor == NULL || cur_page->neighbors[i].distance < closest_neighbor->distance) {
                    closest_neighbor = &page->neighbors[i];
                }
                if (cur_page->neighbors[i].time_diff + tot_time_diff > mig_move_time + mig_queue_time) {
                    // Far enough into future to migrate
                    pred_pages[(*idx)++] = cur_page->neighbors[i].page;
                }
            }
        }
        if (closest_neighbor == NULL || closest_neighbor->page == NULL) break;
        cur_page = closest_neighbor->page;
        tot_time_diff += closest_neighbor->time_diff;
    }

#endif

}
