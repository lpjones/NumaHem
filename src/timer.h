#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

struct timespec get_time();
double elapsed_time(struct timespec start, struct timespec end);
uint64_t rdtscp(void);