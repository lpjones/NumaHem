#include "pebs.h"

// Public variables


// Private variables
static int pfd[PEBS_NPROCS][NPBUFTYPES];
static struct perf_event_mmap_page *perf_page[PEBS_NPROCS][NPBUFTYPES];
static FILE* trace_fp = NULL;
static _Atomic bool kill_internal_threads[NUM_INTERNAL_THREADS];
static pthread_t internal_threads[NUM_INTERNAL_THREADS];

struct perf_sample {
  __u64	ip;             /* if PERF_SAMPLE_IP*/
//   __u32 pid, tid;       /* if PERF_SAMPLE_TID */
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

struct pebs_stats {
    uint64_t throttles, unthrottles;
    uint64_t local_accesses, remote_accesses;
    uint64_t internal_mem_overhead;
    uint64_t unknown_samples;
};

struct pebs_stats pebs_stats = {0};


void wait_for_threads() {
    for (int i = 0; i < NUM_INTERNAL_THREADS; i++) {
        void *ret;
        pthread_join(internal_threads[i], &ret);
    }
    printf("Internal threads killed\n");
}

void pebs_cleanup() {
    
}

static inline void kill_thread(uint8_t thread) {
    atomic_store(&kill_internal_threads[thread], true);
}

void kill_threads() {
    printf("Killing threads\n");
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

  attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_ADDR; // PERF_SAMPLE_TID, PERF_SAMPLE_WEIGHT
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
  struct perf_event_mmap_page *p = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, pfd[cpu_idx][type], 0);
  assert(p != MAP_FAILED);
  pebs_stats.internal_mem_overhead += mmap_size;
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
        LOG_STATS("internal_mem_overhead: [%lu]\tthrottles/unthrottles: [%lu/%lu]\tunknown_samples: [%lu]\n", 
                pebs_stats.internal_mem_overhead, pebs_stats.throttles, pebs_stats.unthrottles, pebs_stats.unknown_samples)
    }
    return NULL;
}

static void start_pebs_stats_thread() {
    int s = pthread_create(&internal_threads[PEBS_STATS_THREAD], NULL, pebs_stats_thread, NULL);
    assert(s == 0);
}

void pebs_init(void) {
    printf("in pebs_init\n");

#if PEBS_STATS == 1
    printf("pebs_stats: %d\n", PEBS_STATS);
    start_pebs_stats_thread();
#endif

    

    trace_fp = fopen("trace.bin", "wb");
    if (trace_fp == NULL) {
        perror("miss ratio file fopen");
    }
    assert(trace_fp != NULL);

    int pebs_start_cpu = 0;
    int num_cores = PEBS_NPROCS;
    
    for (int i = pebs_start_cpu; i < pebs_start_cpu + num_cores; i++) {
        perf_page[i][DRAMREAD] = perf_setup(0x1d3, 0, i, i * 2, DRAMREAD);      // MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM, mem_load_uops_l3_miss_retired.local_dram        
        perf_page[i][NVMREAD] = perf_setup(0x4d3, 0, i, i * 2, NVMREAD);     // MEM_LOAD_RETIRED.LOCAL_PMM        
    }
}

struct perf_sample read_perf_sample(uint32_t cpu_idx, uint8_t event) {
    struct perf_event_mmap_page *p = perf_page[cpu_idx][event];

    struct perf_sample rec = {.addr = 0};

    if (p == NULL) return rec;
    char *data = (char*)p + p->data_offset;
    uint64_t data_size = p->data_size;
    if (data_size == 0) return rec;

    uint64_t head = p->data_head;
    uint64_t tail = p->data_tail;
    if (head == tail) return rec;

    assert(tail <= data_size);

    struct perf_event_header *hdr = (struct perf_event_header *)(data + tail);
    assert(tail + hdr->size <= data_size);
    assert(hdr->size != 0);

    switch (hdr->type) {
        case PERF_RECORD_SAMPLE:
            // printf("record sample");
            if (hdr->size - sizeof(struct perf_event_header) != sizeof(struct perf_sample)) {
                // printf("hdr->size: %lu, perf_sample: %lu\n", hdr->size - sizeof(struct perf_event_header), sizeof(struct perf_sample));
                break;
            }
            memcpy(&rec, (char *)hdr + sizeof(struct perf_event_header), sizeof(struct perf_sample));
            // printf("addr: 0x%llx, ip: 0x%llx\n", rec.addr, rec.ip);
            break;
        case PERF_RECORD_THROTTLE:
            pebs_stats.throttles++;
            // printf("throttle/unthrottle: %lu/%lu\n", pebs_stats.throttles, pebs_stats.unthrottles);
            break;
        case PERF_RECORD_UNTHROTTLE:
            pebs_stats.unthrottles++;
            // printf("throttle/unthrottle: %lu/%lu\n", pebs_stats.throttles, pebs_stats.unthrottles);

            break;
        default:
            // printf("unknown sample\n");
            pebs_stats.unknown_samples++;
            break;
    }

    tail += hdr->size;
    p->data_tail = tail;

    if (rec.addr == 0) return rec;
    struct pebs_rec p_rec = {
        .va = rec.addr,
        .ip = rec.ip,
        .cyc = rdtscp(),
        .cpu = cpu_idx,
        .evt = event
    };

    fwrite(&p_rec, sizeof(struct pebs_rec), 1, trace_fp);


    return rec;
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

    uint64_t num_loops = 0;
    for (int i = 0; i < NUM_INTERNAL_THREADS; i++) {
        atomic_store(&kill_internal_threads[i], false);
    }
    while (true) {
        num_loops++;
        // struct timespec start = get_time();
        // printf("killed: %d\n", killed(PEBS_THREAD));
        if (!(num_loops & 0xF) && killed(PEBS_THREAD)) {

            printf("Killing pebs_thread\n");
        // printf("killed: %d\n", killed(PEBS_THREAD));

            break;
        }

        int pebs_start_cpu = 0;
        int num_cores = PEBS_NPROCS;

        for (int i = pebs_start_cpu; i < pebs_start_cpu + num_cores; i++) {
            for(int j = 0; j < NPBUFTYPES; j++) {
                // struct pebs_rec sample = read_perf_sample(i, j);
                read_perf_sample(i, j);
                
            }
        }
    }
    pebs_cleanup();
    return NULL;
}

void start_pebs_thread() {
    int s = pthread_create(&internal_threads[PEBS_THREAD], NULL, pebs_scan_thread, NULL);
    assert(s == 0);
}



