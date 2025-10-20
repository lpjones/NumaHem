#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <malloc.h>


double *a, *b, *c;

double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void init_arrays(uint64_t array_size) {
    srand(0);

    for (size_t i = 0; i < array_size / sizeof(double); i++) {
        a[i] = (double)rand();
        b[i] = (double)rand();
        c[i] = (double)rand();
    }
}

void run_stream_copy(uint64_t array_size, int num_iters) {
    for (int iter = 0; iter < num_iters; iter++) {
        double start = get_time_sec();
        for (size_t i = 0; i < array_size / sizeof(double); i++) {
            c[i] += a[i] + b[i];
        }
        double end = get_time_sec();
        printf("Iter %d: time = %.6f seconds\n", iter, end - start);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: ./stream <array size (MB)> <num iters>\n");
        return 1;
    }

    uint64_t array_size = (uint64_t)(atoi(argv[1])) * 1024 * 1024;
    int num_iters = atoi(argv[2]);

    printf("Allocating arrays...\n");
    printf("Array size: %ld bytes\n", array_size);

    a = calloc(1, array_size);
    b = calloc(1, array_size);
    c = calloc(1, array_size);

    if (!a || !b || !c) {
        fprintf(stderr, "Allocation failed\n");
        return 1;
    }

    printf("a: %p - %p\n", a, (void *)((uintptr_t)a + array_size));
    printf("b: %p - %p\n", b, (void *)((uintptr_t)b + array_size));
    printf("c: %p - %p\n", c, (void *)((uintptr_t)c + array_size));

    printf("Arrays allocated: each %.2f GB\n", array_size / (1024.0 * 1024.0 * 1024.0));

    init_arrays(array_size);

    printf("Starting STREAM-like test...\n");

    double total_start = get_time_sec();

    run_stream_copy(array_size, num_iters);

    double total_end = get_time_sec();
    printf("Avg wall time: %.6f seconds\n", (total_end - total_start) / num_iters);

    free(a);
    free(b);
    free(c);

    printf("Done.\n");
    return 0;
}
