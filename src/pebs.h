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

#include "timer.h"

#ifndef PEBS_SCAN_CPU
    #define PEBS_SCAN_CPU 0
#endif

#ifndef SAMPLE_PERIOD
    #define SAMPLE_PERIOD 100
#endif

#ifndef PERF_PAGES
    #define PERF_PAGES (1 + (1 << 16))
#endif

#ifndef PEBS_NPROCS
    #define PEBS_NPROCS 16
#endif

enum {
    PEBS_THREAD,
    NUM_INTERNAL_THREADS
};


enum pbuftype {
  DRAMREAD = 0,
  NVMREAD = 1,  
  NPBUFTYPES
};

void pebs_init();
void start_pebs_thread();
void wait_for_threads();
void kill_threads();