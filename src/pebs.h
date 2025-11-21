#ifndef _PEBS_HEADER
#define _PEBS_HEADER

#ifndef __USE_GNU
    #define __USE_GNU
#endif
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/ioctl.h>

#include "timer.h"
#include "interpose.h"
#include "logging.h"
#include "spsc-ring.h"
#include "fifo.h"


#ifndef NO_SAMPLE_RESET_TIME
    #define NO_SAMPLE_RESET_TIME 50000000
#endif

#ifndef PEBS_SCAN_CPU
    #define PEBS_SCAN_CPU 2
#endif

#ifndef PEBS_STATS_CPU
    #define PEBS_STATS_CPU 4
#endif

#ifndef MIGRATE_CPU
    #define MIGRATE_CPU 6
#endif

#ifndef SAMPLE_PERIOD
    #define SAMPLE_PERIOD 100
#endif

#ifndef PERF_PAGES
    #define PERF_PAGES (1 + (1 << 4))  // Uses 8GB total for 16 CPUs
#endif

#ifndef PEBS_NPROCS
    #define PEBS_NPROCS 16
#endif

#ifndef HOT_THRESHOLD
    #define HOT_THRESHOLD 8
#endif

#ifndef SAMPLE_COOLING_THRESHOLD
    #define SAMPLE_COOLING_THRESHOLD 100000
#endif

#ifndef CYC_COOL_THRESHOLD
    #define CYC_COOL_THRESHOLD 10000000
#endif

enum {
    PEBS_THREAD,
    PEBS_STATS_THREAD,
    MIGRATE_THREAD,
    NUM_INTERNAL_THREADS
};


enum pbuftype {
  DRAMREAD = 0,
  REMREAD = 1,  
  NPBUFTYPES
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
    uint64_t internal_mem_overhead, mem_allocated;
    uint64_t unknown_samples;
    uint64_t wrapped_records;
    uint64_t wrapped_headers;
    uint64_t dram_accesses, rem_accesses;
    uint64_t promotions, demotions;
    uint64_t pebs_resets;
    uint64_t non_tracked_mem;

};

extern struct pebs_stats pebs_stats;


void pebs_init();
void start_pebs_thread();
void wait_for_threads();
void kill_threads();

#endif