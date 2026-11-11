
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>

#define BSS_SIZE (64UL * 1024UL * 1024UL)

static unsigned char bss_array[BSS_SIZE];

int main(void)
{
    struct timespec start, end;
    struct rusage before, after;

    printf("====================================\n");
    printf("         BSS MEMORY TEST\n");
    printf("====================================\n");

    if (getrusage(RUSAGE_SELF, &before) != 0) {
        perror("getrusage");
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);

    if (page_size == 0) {
        fprintf(stderr, "Invalid page size\n");
        return EXIT_FAILURE;
    }

    volatile unsigned long checksum = 0;

    size_t page_count = 0;

    for (size_t i = 0; i < BSS_SIZE; i += page_size) {
        if (bss_array[i] != 0) {
            fprintf(stderr, "BSS zero initialization failed\n");
            return EXIT_FAILURE;
        }

        bss_array[i] = 1;
        checksum += bss_array[i];
        page_count++;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    if (getrusage(RUSAGE_SELF, &after) != 0) {
        perror("getrusage");
        return EXIT_FAILURE;
    }

    double elapsed =
        (double)(end.tv_sec - start.tv_sec) +
        (double)(end.tv_nsec - start.tv_nsec) / 1e9;

    printf("BSS size         : %lu MB\n",
           (unsigned long)(BSS_SIZE / (1024UL * 1024UL)));

    printf("Page size        : %zu bytes\n", page_size);
    printf("Touched pages    : %zu\n", page_count);
    printf("Execution time   : %.6f seconds\n", elapsed);

    printf("Minor page faults: %ld\n",
           after.ru_minflt - before.ru_minflt);

    printf("Major page faults: %ld\n",
           after.ru_majflt - before.ru_majflt);

    printf("Checksum         : %lu\n", checksum);
    printf("BSS TEST PASSED\n");

    return EXIT_SUCCESS;
}
