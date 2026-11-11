#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>

#define MEMORY_SIZE (64UL * 1024UL * 1024UL)

static double elapsed_seconds(
    struct timespec start,
    struct timespec end
) {
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(void) {
    struct timespec start, end;
    struct rusage before, after;

    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *memory = NULL;
    volatile uint64_t checksum = 0;

    if (page_size == 0) {
        fprintf(stderr, "Invalid page size\n");
        return EXIT_FAILURE;
    }

    if (getrusage(RUSAGE_SELF, &before) != 0) {
        perror("getrusage");
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    memory = malloc(MEMORY_SIZE);
    if (memory == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    /* Sequential write: touch every page */
    for (size_t i = 0; i < MEMORY_SIZE; i += page_size) {
        memory[i] = (unsigned char)(i / page_size);
    }

    /* Sequential read */
    for (size_t i = 0; i < MEMORY_SIZE; i += page_size) {
        checksum += memory[i];
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (getrusage(RUSAGE_SELF, &after) != 0) {
        perror("getrusage");
        free(memory);
        return EXIT_FAILURE;
    }

    printf("====================================\n");
    printf("    SEQUENTIAL ACCESS TEST\n");
    printf("====================================\n");
    printf("Memory size      : 64 MB\n");
    printf("Page size        : %zu bytes\n", page_size);
    printf("Touched pages    : %zu\n",
           (MEMORY_SIZE + page_size - 1) / page_size);
    printf("Execution time   : %.6f seconds\n",
           elapsed_seconds(start, end));
    printf("Minor page faults: %ld\n",
           after.ru_minflt - before.ru_minflt);
    printf("Major page faults: %ld\n",
           after.ru_majflt - before.ru_majflt);
    printf("Checksum         : %llu\n",
           (unsigned long long)checksum);

    free(memory);

    printf("Memory released  : PASS\n");
    printf("SEQUENTIAL ACCESS TEST PASSED\n");

    return EXIT_SUCCESS;
}