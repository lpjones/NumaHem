#include <stdio.h>
#include <numa.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#define BUFFER 64   // add 48MB to free size since it usually eats more than it should

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
    numa_set_preferred(node);
    mmap(NULL, eat_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    while (1);
}