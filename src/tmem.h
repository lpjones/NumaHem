#ifndef _TMEM_HEADER
#define _TMEM_HEADER

#include <stdio.h>
#include <numa.h>
#include <numaif.h>

#include "pebs.h"
#include "uthash.h"
#include "algorithm.h"

// #define DRAM_SIZE (14 * (1024UL * 1024UL * 1024UL))
// #define REMOTE_SIZE (6 * (1024UL * 1024UL * 1024UL))

#define DRAM_NODE 0
#define REM_NODE 1

// #define PAGE_SIZE 4096UL              // 4KB
// #define PAGE_SIZE (1 * (1024UL * 1024UL))
#ifndef PAGE_SIZE
#define PAGE_SIZE (2 * (1024UL * 1024UL)) // 2MB
#endif
// #define PAGE_SIZE (256 * 1024UL) // 256KB
#define BASE_PAGE_SIZE 4096UL

#define PAGE_MASK (~(PAGE_SIZE - 1))
#define BASE_PAGE_MASK (~(BASE_PAGE_SIZE - 1))

// Use either DRAM_BUFFER or DRAM_SIZE
// #define DRAM_BUFFER (1 * 1024L * 1024L * 1024L)     // How much to leave available on DRAM node

#define DRAM_SIZE (2 * 1024L * 1024L * 1024L)


extern struct fifo_list hot_list;
extern struct fifo_list cold_list;
extern struct fifo_list free_list;

extern long dram_free;
extern long dram_size;
extern long dram_used;
extern pthread_mutex_t mmap_lock;
extern _Atomic bool dram_lock;

enum {
    IN_DRAM,
    IN_REM
};

#ifndef MAX_NEIGHBORS
#define MAX_NEIGHBORS 4
#endif

struct tmem_page;

struct neighbor_page {
    struct tmem_page *page;
    double distance;
    uint64_t time_diff;
};

struct tmem_page {
    uint64_t va;
    void* va_start;
    uint64_t size;
    uint64_t mig_up, mig_down;
    uint64_t accesses;
    uint64_t local_clock;
    uint64_t cyc_accessed;
    uint64_t ip;
    uint64_t mig_start;
    pthread_mutex_t page_lock;

    UT_hash_handle hh;
    struct tmem_page *next, *prev;
    struct neighbor_page neighbors[MAX_NEIGHBORS];
    struct fifo_list *list;

    // Page states
    _Atomic uint8_t in_dram;
    _Atomic bool hot;
    _Atomic bool free;
    _Atomic bool migrating;
    _Atomic bool migrated;
};

void tmem_init();
void* tmem_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int tmem_munmap(void *addr, size_t length);
void tmem_cleanup();
struct tmem_page* find_page(uint64_t va);
struct tmem_page* find_page_no_lock(uint64_t va);

#endif