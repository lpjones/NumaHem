#include "pebs.h"

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
        LOG_STATS("\tdram_free: [%ld]\tdram_used: [%ld]\t dram_size: [%ld]\tdram_cap: [%ld]\n", dram_free, dram_used, dram_size, dram_free - DRAM_BUFFER);
        // hacky way to update dram_free every second in case there's drift over time
        long cur_dram_free;
        numa_node_size(DRAM_NODE, &cur_dram_free);
        dram_free = cur_dram_free + dram_used;
    }
    return NULL;
}

static void start_pebs_stats_thread() {
    int s = pthread_create(&internal_threads[PEBS_STATS_THREAD], NULL, pebs_stats_thread, NULL);
    assert(s == 0);
}

void pebs_init(void) {
    internal_call = true;

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

    internal_call = false;
}

struct perf_sample read_perf_sample(uint32_t cpu_idx, uint8_t event) {
    struct perf_event_mmap_page *p = perf_page[cpu_idx][event];
    struct perf_sample rec = { .addr = 0 };
    if (!p) return rec;

    char *data = (char*)p + p->data_offset;
    uint64_t data_size = p->data_size;
    assert(((data_size - 1) & data_size) == 0); // ensure data_size is power of 2
    if (data_size == 0) return rec;

    uint64_t head = atomic_load_explicit(&p->data_head, memory_order_acquire);
    uint64_t tail = p->data_tail;
    if (head == tail) return rec;

    uint64_t avail = head - tail;
    if (avail < sizeof(struct perf_event_header)) return rec;

    uint64_t mask = data_size - 1;  // data_size is power of 2
    uint64_t hdr_idx = tail & mask;

    /* copy header (handles header split) */
    struct perf_event_header hdr_tmp;
    if (hdr_idx + sizeof(hdr_tmp) <= data_size) {
        memcpy(&hdr_tmp, data + hdr_idx, sizeof(hdr_tmp));
    } else {
        pebs_stats.wrapped_headers++;
        uint64_t first = data_size - hdr_idx;
        memcpy(&hdr_tmp, data + hdr_idx, first);
        memcpy((char*)&hdr_tmp + first, data, sizeof(hdr_tmp) - first);
    }

    if (hdr_tmp.size == 0) return rec;

    if (avail < hdr_tmp.size) return rec; /* not fully published yet */

    /* If the record body is contiguous (no wrap), handle it quickly.
       Otherwise, drop it. */
    uint64_t rec_idx = hdr_idx;
    uint32_t rsize = hdr_tmp.size;
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
    atomic_store_explicit(&p->data_tail, tail, memory_order_release);

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


void make_hot_request(struct tmem_page* page) {

}

void make_cold_request(struct tmem_page* page) {

}


void* pebs_scan_thread() {
    internal_call = true;
    // set cpu
    // cpu_set_t cpuset;
    // CPU_ZERO(&cpuset);
    // CPU_SET(PEBS_SCAN_CPU, &cpuset);
    // // pthread_t thread_id = pthread_self();
    // int s = pthread_setaffinity_np(internal_threads[PEBS_THREAD], sizeof(cpu_set_t), &cpuset);
    // assert(s == 0);
    // pebs_init();
    printf("pebs scan started\n");

    uint64_t samples_since_cool = 0;

    uint64_t num_loops = 0;
    for (int i = 0; i < NUM_INTERNAL_THREADS; i++) {
        atomic_store(&kill_internal_threads[i], false);
    }
    while (true) {
        num_loops++;
        // struct timespec start = get_time();
        // printf("killed: %d\n", killed(PEBS_THREAD));
        if (!(num_loops & 0xF) && killed(PEBS_THREAD)) {

            LOG_DEBUG("Killing pebs_thread\n");
        // printf("killed: %d\n", killed(PEBS_THREAD));

            break;
        }

        int pebs_start_cpu = 0;
        int num_cores = PEBS_NPROCS;

        for (int cpu_idx = pebs_start_cpu; cpu_idx < pebs_start_cpu + num_cores; cpu_idx++) {
            for(int evt = 0; evt < NPBUFTYPES; evt++) {
                samples_since_cool++;
                // struct pebs_rec sample = read_perf_sample(i, j);
                struct perf_sample sample = read_perf_sample(cpu_idx, evt);
                if (sample.addr == 0) continue;

                uint64_t addr_aligned = sample.addr & PAGE_MASK;
                struct tmem_page *page = find_page(addr_aligned);

                // Try 4KB aligned page if not 2MB aligned page
                if (page == NULL)
                    page = find_page(sample.addr & BASE_PAGE_MASK);
                if (page == NULL) continue;

                // long move_pages(pid, count, void *pages[count], int nodes[count], int status[count], int flags);
                // int node;
                // void* pages = (void *)page->va;
                // int ret = numa_move_pages(0, 1, &pages, NULL, &node, 0);
                // if (ret == -1) {
                //     perror("numa_move_pages");
                //     LOG_DEBUG("numa_move_pages failed\n");
                // }
                // if (node < 0) {
                //     fprintf(stderr, "error = %d (%s)\n", node, strerror(-node));
                //     assert(0);
                // } else {
                //     printf("PEBS: node %d: addr: 0x%lx\n", node, page->va);
                // }

                page->accesses = (page->accesses + 1) & (MAX_ACCESSES);    // cap accesses at 255
                // algorithm
                if (page->accesses >= HOT_THRESHOLD) {
                    make_hot_request(page);
                } else {
                    make_cold_request(page);
                }

                // cool off
                page->accesses >>= (global_clock - page->local_clock);
                page->local_clock = global_clock;

                if (samples_since_cool >= SAMPLE_COOLING_THRESHOLD) {
                    global_clock++;
                    samples_since_cool = 0;
                }
            }
        }
    }
    pebs_cleanup();
    return NULL;
}

// Some pages might not have faulted yet and don't exist
// So use mbind to set future faults to be on the right numa node
// and use the flag MPOL_MF_MOVE to move the pages that do exist to the numa node
void tmem_migrate_pages(struct tmem_page** pages, uint64_t num_pages, int node) {
    unsigned long nodemask = 1UL << node;

    void *addrs[PAGE_SIZE / BASE_PAGE_SIZE];
    for (uint64_t page_idx = 0; page_idx < num_pages; page_idx++) {
        
    }
    // mbind()

    // char *mem;
	// unsigned long nodemask = 1UL << node;

	// mem = libc_mmap(addr, length, prot, flags, fd, offset);  
	// if (mem == (char *)-1)
	// 	mem = NULL;
	// else 
	// 	mbind(mem, length, MPOL_BIND, &nodemask, 64, 0);

    // return mem; 
}


/*
    Reads in hot requests from pebs_scan_thread
    For each hot request it checks if it's still hot and is in remote dram,
    if it is then it migrates a cold dram page to remote dram
    if there's no cold dram page it starts over (continues)
    then it migrates the the hot remote page(s) to dram
*/
void *migration_thread() {
    internal_call = true;

    uint64_t num_loops = 0;
    while (true) {
        num_loops++;
        if (!(num_loops & 0xF) && killed(MIGRATION_THREAD)) {
            break;
        }

        // Check if any hot pages in remote memory
        // and any cold pages in dram to swap them
        if (hot_list.numentries == 0) continue;
        if (cold_list.numentries == 0) continue;

        struct tmem_page *hot_page = dequeue_fifo(&hot_list);
        while (hot_page != NULL && !hot_page->hot) {
            hot_page = dequeue_fifo(&hot_list);
        }
        if (hot_page == NULL) continue;

        uint64_t hot_page_bytes = hot_page->size;

        // Find enough cold pages in dram to match at least hot page size for swap
        uint64_t cold_page_bytes = 0;
        uint64_t num_cold_pages = 0;
        struct tmem_page *cold_pages[PAGE_SIZE / BASE_PAGE_SIZE];
        
        while (cold_page_bytes < hot_page->size) {
            struct tmem_page *cold_page = dequeue_fifo(&cold_list);
            if (cold_page == NULL) break;
            if (cold_page->in_dram == IN_REM || cold_page->hot) continue;

            cold_page_bytes += cold_page->size;
            cold_pages[num_cold_pages++] = cold_page;
            
        }
        if (cold_page_bytes < hot_page->size) continue; // TODO: requeue hot_page to front of queue somehow

        tmem_migrate_pages(cold_pages, num_cold_pages, REM_NODE);
        tmem_migrate_pages(&hot_page, 1, DRAM_NODE);
        



        // swap them (migrate cold page from dram to remote)
        // then migrate hot page from remote to dram.
        // set preferred allocation to remote while migration
        // is happening so mmaps don't take up migration spot
        // of hot page before it can migrate
        // migrate_page(cold_page, REM_NODE);
        // migrate_page(hot_page, DRAM_NODE);
        

    }
    LOG_DEBUG("Migration thread killed\n");
    return NULL;
}

void start_pebs_thread() {
    int s = pthread_create(&internal_threads[PEBS_THREAD], NULL, pebs_scan_thread, NULL);
    assert(s == 0);
}

void start_migrate_thread() {
    int s = pthread_create(&internal_threads[MIGRATION_THREAD], NULL, migration_thread, NULL);
    assert(s == 0);
}

