#include "algorithm.h"

#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define VA_WEIGHT 0
#define CYC_WEIGHT 1
#define IP_WEIGHT 0

struct tmem_page *page_history[HISTORY_SIZE];
uint32_t page_his_idx = 0;

// static double algo_threshold = 0;
static double avg_dist = 1;
// static uint64_t dist_count = 1;
static double decay = 0.1;

static double calc_distance(struct tmem_page *a, struct tmem_page *b) {
    double distance = 0;
    // double x = 5;
    // printf("%f -> %f\n", (double)(a->va) - (double)(b->va), ABS((double)(a->va) - (double)(b->va)));
    double va_diff = ABS((double)(a->va) - (double)(b->va));
    double cyc_diff = ABS((double)(a->cyc_accessed) - (double)(b->cyc_accessed));
    double ip_diff = ABS((double)(a->ip) - (double)(b->ip));

    distance += va_diff * VA_WEIGHT;
    distance += cyc_diff * CYC_WEIGHT;
    distance += ip_diff * IP_WEIGHT;

    if (distance == 0) return avg_dist;

    // avg_dist = (avg_dist * (1.0 * (dist_count - 1) / dist_count) + distance * (1.0 / dist_count)); // equal weighting for all samples
    avg_dist = decay * distance + (1.0 - decay) * avg_dist;
    // dist_count++;
    // printf("avg_dist: %f\n", avg_dist);

    return distance;
}

static void update_neighbors(struct tmem_page *old_page) {
    for (uint32_t i = 0; i < HISTORY_SIZE; i++) {
        struct tmem_page *cur_page = page_history[i];
        if (cur_page == old_page) continue;

        double distance = calc_distance(old_page, cur_page);
        assert(distance != 0);
        
        // Find empty spot or furthest distance neighbor O(MAX_NEIGHBORS)
        struct neighbor_page *furthest_neighbor = NULL;
        for (uint32_t j = 0; j < MAX_NEIGHBORS; j++) {
            if (old_page->neighbors[j].page == NULL) {  // empty spot
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

    // Calc distances for history window and replace neighbors if better

    // O(HISTORY_SIZE * MAX_NEIGHBORS)
    
}

struct tmem_page* algo_predict_page(struct tmem_page *page) {
    return NULL;
    if (pebs_stats.throttles > pebs_stats.unthrottles) return NULL;

    // return closest neighbor if below threshold
    struct neighbor_page *close_neighbor = &page->neighbors[0];
    for (uint32_t i = 1; i < MAX_NEIGHBORS; i++) {
        if (close_neighbor->distance == 0 || page->neighbors[i].distance < close_neighbor->distance) {
            close_neighbor = &page->neighbors[i];
        }
    }

    if (close_neighbor->page == NULL) return NULL;
    
    // uint64_t pebs_throttles = pebs_stats.throttles - pebs_stats.unthrottles;
    // printf("threshold normalizer: %lu\n", (500 * SAMPLE_PERIOD * (pebs_throttles)));
    if (close_neighbor->distance < avg_dist / 10000) {
        return close_neighbor->page;
    }

    //(500 * SAMPLE_PERIOD * (pebs_stats.throttles - pebs_stats.unthrottles + 1))
    return NULL;
}