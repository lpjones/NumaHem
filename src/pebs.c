#include "pebs.h"

#define CHECK_KILLED(thread) if (!(num_loops++ & 0xF) && killed(thread)) return NULL;


// Public variables


// Private variables
static int pfd[PEBS_NPROCS][NPBUFTYPES];
static struct perf_event_mmap_page *perf_page[PEBS_NPROCS][NPBUFTYPES];
static FILE* trace_fp = NULL;
static FILE* tmem_trace_fp = NULL;
static _Atomic bool kill_internal_threads[NUM_INTERNAL_THREADS];
static pthread_t internal_threads[NUM_INTERNAL_THREADS];

static uint64_t global_clock = 0;


struct perf_sample {
  __u64	ip;             /* if PERF_SAMPLE_IP*/
//   __u32 pid, tid;       /* if PERF_SAMPLE_TID */
  __u64 time;           /* if PERF_SAMPLE_TIME */
  __u64 addr;           /* if PERF_SAMPLE_ADDR */
//   __u64 weight;         /* if PERF_SAMPLE_WEIGHT */
// __u64 data_src;         /* if PERF_SAMPLE_DATA_SRC */
};

struct pebs_rec {
  uint64_t cyc;
  uint64_t va;
  uint64_t ip;
  uint32_t cpu;
  uint8_t  evt;
} __attribute__((packed));

struct pebs_stats pebs_stats = {0};


void wait_for_threads() {
    for (int i = 0; i < NUM_INTERNAL_THREADS; i++) {
        void *ret;
        pthread_join(internal_threads[i], &ret);
    }
    LOG_DEBUG("Internal threads killed\n");
}

void pebs_cleanup() {
    
}

static inline void kill_thread(uint8_t thread) {
    atomic_store(&kill_internal_threads[thread], true);
}

void kill_threads() {
    LOG_DEBUG("Killing threads\n");
    for (int i = 0; i < NUM_INTERNAL_THREADS; i++) {
        kill_thread(i);
    }
}

static inline bool killed(uint8_t thread) {
    return atomic_load(&kill_internal_threads[thread]);
}

static inline long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    int ret;
    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
  return ret;
}

static struct perf_event_mmap_page* perf_setup(__u64 config, __u64 config1, uint32_t cpu_idx, __u64 cpu, __u64 type) {
    struct perf_event_attr attr = {0};

    attr.type = PERF_TYPE_RAW;
    attr.size = sizeof(struct perf_event_attr);

    attr.config = config;
    attr.config1 = config1;
    attr.sample_period = SAMPLE_PERIOD;

    attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TIME | PERF_SAMPLE_ADDR; // PERF_SAMPLE_TID, PERF_SAMPLE_WEIGHT
    attr.disabled = 0;
    //attr.inherit = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.exclude_callchain_kernel = 1;
    attr.exclude_callchain_user = 1;
    attr.precise_ip = 1;
    
    pfd[cpu_idx][type] = perf_event_open(&attr, -1, cpu, -1, 0);
    assert(pfd[cpu_idx][type] != -1);


    size_t mmap_size = sysconf(_SC_PAGESIZE) * PERF_PAGES;
    /* printf("mmap_size = %zu\n", mmap_size); */
    struct perf_event_mmap_page *p = libc_mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, pfd[cpu_idx][type], 0);
    LOG_DEBUG("PEBS: cpu: %u, type: %llu, buffer size: %lu\n", cpu_idx, type, mmap_size);
    pebs_stats.internal_mem_overhead += mmap_size;

    assert(p != MAP_FAILED);
    fprintf(stderr, "Set up perf on core %llu\n", cpu);


    return p;
}

void* pebs_stats_thread() {
    internal_call = true;

    // cpu_set_t cpuset;
    // CPU_ZERO(&cpuset);
    // CPU_SET(PEBS_STATS_CPU, &cpuset);
    // int s = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    // assert(s == 0);
    // return NULL;
    while (!killed(PEBS_STATS_THREAD)) {
        sleep(1);
        LOG_STATS("internal_mem_overhead: [%lu]\tmem_allocated: [%lu]\tthrottles/unthrottles: [%lu/%lu]\tunknown_samples: [%lu]\n", 
                pebs_stats.internal_mem_overhead, pebs_stats.mem_allocated, pebs_stats.throttles, pebs_stats.unthrottles, pebs_stats.unknown_samples)
        LOG_STATS("\twrapped_records: [%lu]\twrapped_headers: [%lu]\n", 
                pebs_stats.wrapped_records, pebs_stats.wrapped_headers);

#ifdef DRAM_BUFFER
        LOG_STATS("\tdram_free: [%ld]\tdram_used: [%ld]\t dram_size: [%ld]\tdram_cap: [%ld]\n", dram_free, dram_used, dram_size, dram_free - DRAM_BUFFER);
#endif
#ifdef DRAM_SIZE
        LOG_STATS("\tdram_used: [%ld]\t dram_size: [%ld]\n", dram_used, dram_size);
#endif
        double percent_dram = 100.0 * pebs_stats.dram_accesses / (pebs_stats.dram_accesses + pebs_stats.rem_accesses);
        LOG_STATS("\tdram_accesses: [%ld]\trem_accesses: [%ld]\t percent dram: [%.2f]\n", 
            pebs_stats.dram_accesses, pebs_stats.rem_accesses, percent_dram);
        
        uint64_t migrations = pebs_stats.promotions + pebs_stats.demotions;
        LOG_STATS("\tpromotions: [%lu]\tdemotions: [%lu]\tmigrations: [%lu]\n", 
                pebs_stats.promotions, pebs_stats.demotions, migrations);

        pebs_stats.dram_accesses = 0;
        pebs_stats.rem_accesses = 0;
        pebs_stats.promotions = 0;
        pebs_stats.demotions = 0;

#ifdef DRAM_BUFFER
        // hacky way to update dram_size every second in case there's drift over time
        long cur_dram_free;
        dram_size = numa_node_size(DRAM_NODE, &cur_dram_free);
        dram_size -= cur_dram_free; 
#endif
    }
    return NULL;
}

static void start_pebs_stats_thread() {
    int s = pthread_create(&internal_threads[PEBS_STATS_THREAD], NULL, pebs_stats_thread, NULL);
    assert(s == 0);
}



struct perf_sample read_perf_sample(uint32_t cpu_idx, uint8_t event) {
    struct perf_event_mmap_page *p = perf_page[cpu_idx][event];
    struct perf_sample rec = { .addr = 0 };
    if (!p) return rec;

    char *data = (char*)p + p->data_offset;
    uint64_t data_size = p->data_size;
    assert(((data_size - 1) & data_size) == 0); // ensure data_size is power of 2
    if (data_size == 0) return rec;

    // uint64_t head = atomic_load_explicit(&p->data_head, memory_order_acquire);
    uint64_t head = __atomic_load_n(&p->data_head, __ATOMIC_ACQUIRE);
    uint64_t tail = p->data_tail;
    if (head == tail) return rec;

    uint64_t avail = head - tail;
    if (avail < sizeof(struct perf_event_header)) return rec;

    uint64_t mask = data_size - 1;  // data_size is power of 2
    uint64_t hdr_idx = tail & mask;

    /* copy header (handles header split) */
    struct perf_event_header *hdr_tmp;
    if (hdr_idx + sizeof(hdr_tmp) <= data_size) {
        hdr_tmp = (struct perf_event_header *)(data + hdr_idx);
        // memcpy(&hdr_tmp, data + hdr_idx, sizeof(hdr_tmp));
    } else {
        pebs_stats.wrapped_headers++;
        uint64_t first = data_size - hdr_idx;
        memcpy(&hdr_tmp, data + hdr_idx, first);
        memcpy((char*)&hdr_tmp + first, data, sizeof(hdr_tmp) - first);
    }

    if (hdr_tmp->size == 0) return rec;

    if (avail < hdr_tmp->size) return rec; /* not fully published yet */

    /* If the record body is contiguous (no wrap), handle it quickly.
       Otherwise, drop it. */
    uint64_t rec_idx = hdr_idx;
    uint32_t rsize = hdr_tmp->size;
    if (rec_idx + rsize <= data_size) {
        /* contiguous: parse directly from buffer (no heap) */
        struct perf_event_header *hdr = (struct perf_event_header *)(data + rec_idx);
        switch (hdr->type) {
            case PERF_RECORD_SAMPLE:
                if (rsize - sizeof(struct perf_event_header) == sizeof(struct perf_sample)) {
                    memcpy(&rec, data + rec_idx + sizeof(*hdr), sizeof(struct perf_sample));
                }
                break;
            case PERF_RECORD_THROTTLE:
                pebs_stats.throttles++;
                break;
            case PERF_RECORD_UNTHROTTLE:
                pebs_stats.unthrottles++;
                break;
            default:
                pebs_stats.unknown_samples++;
                break;
        }
    } else {
        pebs_stats.wrapped_records++;
    }
    tail += rsize;
    // atomic_store_explicit(&p->data_tail, tail, memory_order_release);
    __atomic_store_n(&p->data_tail, tail, __ATOMIC_RELEASE);

    if (rec.addr != 0) {
        struct pebs_rec p_rec = {
            .va = rec.addr,
            .ip = rec.ip,
            .cyc = rec.time,
            .cpu = cpu_idx,
            .evt = event
        };
        fwrite(&p_rec, sizeof(struct pebs_rec), 1, trace_fp);
    }
    return rec;
}

// Could be munmapped at any time
void make_hot_request(struct tmem_page* page) {
    if (page == NULL) return;
    // page could be munmapped here (but pages are never actually
    // unmapped so just check if it's in free state once locked)
    pthread_mutex_lock(&page->page_lock);
    // check if unmapped
    if (page->free) {
        // printf("Page was free\n");
        pthread_mutex_unlock(&page->page_lock);
        return;
    }
    page->hot = true;
    
    // add to hot list if:
    // page is not already in hot list and in remote mem
    if (page->list != &hot_list && page->in_dram == IN_REM) {
        // either was in remote mem or just got dequeued
        // from cold list in migrate thread
        // page->list == &cold_list and in Remote
        if (page->list != NULL) {
            assert(page->list == &cold_list);
            page_list_remove_page(&cold_list, page);
        }
        assert(page->list == NULL);
        enqueue_fifo(&hot_list, page);

    }
    // printf("page is either already in hot list or is in remote memory\n");
    
    pthread_mutex_unlock(&page->page_lock);

}

void make_cold_request(struct tmem_page* page) {
    if (page == NULL) return;
    // page could be munmapped here (but pages are never actually
    // unmapped so just check if it's in free state once locked)
    pthread_mutex_lock(&page->page_lock);
    // check if unmapped
    if (page->free) {
        pthread_mutex_unlock(&page->page_lock);
        return;
    }
    page->hot = false;
    // move to cold list if:
    // page is not already in cold list and
    // page is in dram
    if (page->list != &cold_list && page->in_dram == IN_DRAM) {
        // remove from hot list
        if (page->list != NULL) {
            assert(page->list == &hot_list);
            page_list_remove_page(&hot_list, page);
        }
        assert(page->list == NULL);
        enqueue_fifo(&cold_list, page);
    }
    
    pthread_mutex_unlock(&page->page_lock);
}


void* pebs_scan_thread() {
    internal_call = true;
    // set cpu
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(PEBS_SCAN_CPU, &cpuset);
    // pthread_t thread_id = pthread_self();
    int s = pthread_setaffinity_np(internal_threads[PEBS_THREAD], sizeof(cpu_set_t), &cpuset);
    assert(s == 0);
    // pebs_init();

    // uint64_t samples_since_cool = 0;
    uint64_t last_cyc_cool = rdtscp();

    uint64_t num_loops = 0;
    
    while (true) {
        // struct timespec start = get_time();
        // printf("killed: %d\n", killed(PEBS_THREAD));
        CHECK_KILLED(PEBS_THREAD);


        int pebs_start_cpu = 0;
        int num_cores = PEBS_NPROCS;

        for (int cpu_idx = pebs_start_cpu; cpu_idx < pebs_start_cpu + num_cores; cpu_idx++) {
            for(int evt = 0; evt < NPBUFTYPES; evt++) {
                // samples_since_cool++;
                // struct pebs_rec sample = read_perf_sample(i, j);
                struct perf_sample sample = read_perf_sample(cpu_idx, evt);
                continue;
                if (sample.addr == 0) continue;

                uint64_t addr_aligned = sample.addr & PAGE_MASK;
                struct tmem_page *page = find_page(addr_aligned);

                // Try 4KB aligned page if not 2MB aligned page
                if (page == NULL)
                    page = find_page(sample.addr & BASE_PAGE_MASK);
                if (page == NULL) continue;

                struct pebs_rec p_rec = {
                    .va = sample.addr,
                    .ip = sample.ip,
                    .cyc = sample.time,
                    .cpu = cpu_idx,
                    .evt = evt
                };
                fwrite(&p_rec, sizeof(struct pebs_rec), 1, tmem_trace_fp);

                if (page->migrated) {
                    LOG_DEBUG("PEBS: accessed migrated page: 0x%lx\n", page->va);
                }
                

                if (evt == DRAMREAD) pebs_stats.dram_accesses++;
                else pebs_stats.rem_accesses++;
                page->accesses++;

                if (page->accesses >= HOT_THRESHOLD) {
                    LOG_DEBUG("PEBS: Made hot: 0x%lx\n", page->va);
                    make_hot_request(page);
                } else {
                    make_cold_request(page);
                }

                // cool off
                page->accesses >>= (global_clock - page->local_clock);
                page->local_clock = global_clock;

                // if (samples_since_cool >= SAMPLE_COOLING_THRESHOLD) {
                //     global_clock++;
                //     samples_since_cool = 0;
                //     uint64_t cur_cyc = rdtscp();
                //     printf("cyc since last cool: %lu\n", cur_cyc - last_cyc_cool);
                //     last_cyc_cool = cur_cyc;
                // }
                uint64_t cur_cyc = rdtscp();
                if (cur_cyc - last_cyc_cool > CYC_COOL_THRESHOLD) {
                    // global_clock++;
                    __atomic_fetch_add(&global_clock, 1, __ATOMIC_RELEASE);
                    last_cyc_cool = cur_cyc;
                }
            }
        }
    }
    pebs_cleanup();
    return NULL;
}

// void tmem_migrate_page(struct tmem_page *page, int node) {
//     // copy page to tmp location
//     void *p = libc_mmap(NULL, page->size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
//     memcpy(page->va_start, p, page->size);


// }

void tmem_migrate_page(struct tmem_page *page, int node) {
    unsigned long nodemask = 1UL << node;

    if (mbind(page->va_start, page->size, MPOL_BIND, &nodemask, 64, MPOL_MF_MOVE | MPOL_MF_STRICT) == -1) {
        perror("mbind");
        printf("mbind failed %p\n", page->va_start);
    } else {
        if (node == DRAM_NODE) {
            // was migrated to dram
            page->in_dram = IN_DRAM;
            page->hot = true;
            enqueue_fifo(&hot_list, page);
        } else {
            page->in_dram = IN_REM;
            page->hot = false;
        }
    }
}

// Some pages might not have faulted yet and don't exist
// So use mbind to set future faults to be on the right numa node
// and use the flag MPOL_MF_MOVE to move the pages that do exist to the numa node
// void tmem_migrate_pages(struct tmem_page** pages, uint64_t num_pages, int node) {
//     unsigned long nodemask = 1UL << node;

//     for (uint64_t page_idx = 0; page_idx < num_pages; page_idx++) {
//         if (mbind(pages[page_idx]->va_start, pages[page_idx]->size, MPOL_BIND, &nodemask, 64, MPOL_MF_MOVE | MPOL_MF_STRICT) == -1) {
//             perror("mbind");
//             printf("mbind failed %p\n", pages[page_idx]->va_start);
//         } else {
//             if (node == DRAM_NODE) {
//                 // was migrated to dram
//                 pages[page_idx]->in_dram = IN_DRAM;
//                 pages[page_idx]->hot = true;
//                 enqueue_fifo(&hot_list, pages[page_idx]);
//             } else {
//                 pages[page_idx]->in_dram = IN_REM;
//                 pages[page_idx]->hot = false;
//             }
//         }
//     }
// }

/*
    Reads in hot requests from pebs_scan_thread
    For each hot request it checks if it's still hot and is in remote dram,
    if it is then it migrates a cold dram page to remote dram
    if there's no cold dram page it starts over (continues)
    then it migrates the the hot remote page(s) to dram
*/


void *migrate_thread() {
    internal_call = true;
    uint64_t num_loops = 0;

    struct tmem_page *hot_page, *cold_page;
    uint64_t cold_bytes = 0;

    while (true) {
        CHECK_KILLED(MIGRATION_THREAD);

        // Don't do any migrations until hot page comes in
        hot_page = dequeue_fifo(&hot_list);
        if (hot_page == NULL) continue;
        pthread_mutex_lock(&hot_page->page_lock);

        assert(hot_page != NULL);
        if (hot_page->list != NULL || hot_page->in_dram == IN_DRAM) {
            pthread_mutex_unlock(&hot_page->page_lock);
            continue;
        }
        
        LOG_DEBUG("MIG: got hot page: 0x%lx\n", hot_page->va);

        // have a valid hot page. Now get cold pages
        // disable dram mmap temporarily
        atomic_store_explicit(&dram_lock, true, memory_order_release);
        uint64_t bytes_free = dram_size - __atomic_load_n(&dram_used, __ATOMIC_ACQUIRE);

        if (bytes_free >= hot_page->size) {
            LOG_DEBUG("MIG: enough dram: 0x%lx\n", hot_page->va);
            // Enough space in dram, just migrate hot page
            // tmem_migrate_pages(&hot_page, 1, DRAM_NODE);
            tmem_migrate_page(hot_page, DRAM_NODE);
            hot_page->migrated = true;
            pebs_stats.promotions++;
            
            __atomic_fetch_add(&dram_used, hot_page->size, __ATOMIC_RELEASE);
            atomic_store_explicit(&dram_lock, false, memory_order_release);
            LOG_DEBUG("MIG: Finished migration: 0x%lx\n", hot_page->va);

            pthread_mutex_unlock(&hot_page->page_lock);
            continue;
        }

        cold_bytes = 0;
        // Not enough space in dram, demote cold pages until enough space
        while (bytes_free + cold_bytes < hot_page->size) {
            cold_page = dequeue_fifo(&cold_list);   // grabs cold page lock
            if (cold_page == NULL) {
                // cold list is empty, abort
                // requeue hot page (kind of bad idea but oh well)
                // enqueue_fifo(&hot_list, hot_page);
                pthread_mutex_unlock(&hot_page->page_lock);

                // enable dram mmap with updated dram_used
                __atomic_fetch_sub(&dram_used, cold_bytes, __ATOMIC_RELEASE);
                atomic_store_explicit(&dram_lock, true, memory_order_release);

                LOG_DEBUG("MIG: no cold pages, aborting\n");
                break;
            }
            assert(cold_page != NULL);
            pthread_mutex_lock(&cold_page->page_lock);
            if (cold_page->list != NULL || cold_page->in_dram == IN_REM || cold_page->hot) {
                pthread_mutex_unlock(&cold_page->page_lock);
                continue;
            }
            assert(cold_page->in_dram == IN_DRAM && !cold_page->hot);

            // tmem_migrate_pages(&cold_page, 1, REM_NODE);
            tmem_migrate_page(cold_page, REM_NODE);
            cold_page->migrated = true;
            cold_bytes += cold_page->size;
            LOG_DEBUG("MIG: demoted 0x%lx\n", cold_page->va);
            pthread_mutex_unlock(&cold_page->page_lock);
            pebs_stats.demotions++;
        }
        if (cold_page == NULL) continue;
        // now enough space in dram
        LOG_DEBUG("MIG: now enough space: 0x%lx\n", hot_page->va);
        // tmem_migrate_pages(&hot_page, 1, DRAM_NODE);
        tmem_migrate_page(hot_page, DRAM_NODE);
        hot_page->migrated = true;
        pebs_stats.promotions++;

        // enable dram mmap
        __atomic_fetch_add(&dram_used, hot_page->size - cold_bytes, __ATOMIC_RELEASE);
        atomic_store_explicit(&dram_lock, false, memory_order_release);
        LOG_DEBUG("MIG: Finished migration: 0x%lx\n", hot_page->va);

        pthread_mutex_unlock(&hot_page->page_lock);
    }
}

void start_pebs_thread() {
    int s = pthread_create(&internal_threads[PEBS_THREAD], NULL, pebs_scan_thread, NULL);
    assert(s == 0);
}

void start_migrate_thread() {
    int s = pthread_create(&internal_threads[MIGRATION_THREAD], NULL, migrate_thread, NULL);
    assert(s == 0);
}

void pebs_init(void) {
    internal_call = true;

    for (int i = 0; i < NUM_INTERNAL_THREADS; i++) {
        atomic_store(&kill_internal_threads[i], false);
    }

#if PEBS_STATS == 1
    LOG_DEBUG("pebs_stats: %d\n", PEBS_STATS);
    start_pebs_stats_thread();
#endif

    tmem_trace_fp = fopen("tmem_trace.bin", "wb");
    if (tmem_trace_fp == NULL) {
        perror("tmem_trace file fopen");
    }
    assert(tmem_trace_fp != NULL);

    trace_fp = fopen("trace.bin", "wb");
    if (trace_fp == NULL) {
        perror("trace file fopen");
    }
    assert(trace_fp != NULL);

    int pebs_start_cpu = 0;
    int num_cores = PEBS_NPROCS;
    
    for (int i = pebs_start_cpu; i < pebs_start_cpu + num_cores; i++) {
        perf_page[i][DRAMREAD] = perf_setup(0x1d3, 0, i, i * 2, DRAMREAD);      // MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM, mem_load_uops_l3_miss_retired.local_dram        
        perf_page[i][REMREAD] = perf_setup(0x4d3, 0, i, i * 2, REMREAD);     // MEM_LOAD_RETIRED.LOCAL_PMM        
    }

    start_pebs_thread();

    start_migrate_thread();

    internal_call = false;
}