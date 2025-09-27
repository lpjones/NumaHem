#include "tmem.h"

void tmem_init() {
    printf("Setting preferred allocation to local\n");
    numa_set_localalloc();

    start_pebs_thread();


}

int tmem_mmap() {
    return 0;
}

int tmem_munmap() {
    return 0;
}

void tmem_cleanup() {
    kill_threads();
    wait_for_threads();
}