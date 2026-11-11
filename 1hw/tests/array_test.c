
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>

#define ARRAY_SIZE (64UL * 1024UL * 1024UL)

static double elapsed_seconds(
    struct timespec start,
    struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(void)
{
    unsigned char *array = NULL;
    struct timespec start, end;
    struct rusage before, after;

    printf("====================================\n");
    printf("       ARRAY MEMORY TEST\n");
    printf("====================================\n");

    if (getrusage(RUSAGE_SELF, &before) != 0) {
        perror("getrusage");
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    array = calloc(ARRAY_SIZE, sizeof(unsigned char));

    if (array == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    /*
     * Sequential write: touch every page.
     */
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);

    if (page_size == 0) {
        fprintf(stderr, "Invalid page size\n");
        free(array);
        return EXIT_FAILURE;
    }

    volatile unsigned long checksum = 0;

    for (size_t i = 0; i < ARRAY_SIZE; i += page_size) {
        array[i] = (unsigned char)(i / page_size);
        checksum += array[i];
    }

    /*
     * Read the initialized values.
     */
    for (size_t i = 0; i < ARRAY_SIZE; i += page_size)
        checksum += array[i];

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        free(array);
        return EXIT_FAILURE;
    }

    if (getrusage(RUSAGE_SELF, &after) != 0) {
        perror("getrusage");
        free(array);
        return EXIT_FAILURE;
    }

    printf("Array size       : %lu MB\n",
           (unsigned long)(ARRAY_SIZE / (1024UL * 1024UL)));

    printf("Page size        : %zu bytes\n", page_size);

    printf("Touched pages    : %zu\n",
           (ARRAY_SIZE + page_size - 1) / page_size);

    printf("Execution time   : %.6f seconds\n",
           elapsed_seconds(start, end));

    printf("Minor page faults: %ld\n",
           after.ru_minflt - before.ru_minflt);

    printf("Major page faults: %ld\n",
           after.ru_majflt - before.ru_majflt);

    printf("Checksum         : %lu\n", checksum);

    free(array);

    printf("Memory released  : PASS\n");
    printf("ARRAY TEST PASSED\n");

    return EXIT_SUCCESS;
}
