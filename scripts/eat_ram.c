#include <stdio.h>
#include <numa.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER 64   // add 64MB to free size since it usually eats more than it should


void protect_from_oom(void) {
    int fd = open("/proc/self/oom_score_adj", O_WRONLY);
    if (fd < 0) {
        perror("open oom_score_adj");
        return;
    }
    if (write(fd, "-1000\n", 6) < 0) {
        perror("write oom_score_adj");
    }
    close(fd);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <RAM left on node (MB)> <node>\n", argv[0]);
        return 1;
    }

    uint64_t free_ram = (strtoull(argv[1], NULL, 10) + BUFFER) << 20;
    int node = atoi(argv[2]);

    long node_free;
    long node_size = numa_node_size(node, &node_free);
    printf("free ram: %lu, node: %i\n", free_ram, node);
    printf("node_free: %li, node_size: %li\n", node_free, node_size);

    uint64_t eat_size = node_free - free_ram;

    printf("eating %lu bytes\n", eat_size);

    protect_from_oom();

    void *p = numa_alloc_onnode(eat_size, node);
    if (!p) { perror("numa_alloc_onnode"); exit(1); }
    /* Touch pages so they are faulted and actually allocated */
    volatile char *q = p;
    for (size_t i = 0; i < eat_size; i += 4096) q[i] = 0;


    // numa_set_preferred(node);
    // mmap(NULL, eat_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    while (1);
}