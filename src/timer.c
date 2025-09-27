#include "timer.h"

struct timespec get_time() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        printf("get_time failed\n");
        assert(0);
    }
    return ts;
}

double elapsed_time(struct timespec start, struct timespec end) {
    long long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL +
                 (end.tv_nsec - start.tv_nsec);
    return elapsed_ns / 1000000000.0;
}

uint64_t rdtscp(void) {
    uint32_t eax, edx;
    // why is "ecx" in clobber list here, anyway? -SG&MH,2017-10-05
    __asm volatile ("rdtscp" : "=a" (eax), "=d" (edx) :: "ecx", "memory");
    return ((uint64_t)edx << 32) | eax;
}