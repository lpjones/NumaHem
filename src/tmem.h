#ifndef _TMEM_HEADER
#define _TMEM_HEADER

#include <stdio.h>
#include <numa.h>

#include "pebs.h"

// #define PAGE_SIZE 4096ULL              // 4KB
#define PAGE_SIZE (2 * (1024UL * 1024UL)) // 2MB

void tmem_init();
int tmem_mmap();
int tmem_munmap();
void tmem_cleanup();

#endif