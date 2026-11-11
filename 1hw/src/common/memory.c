#define _DEFAULT_SOURCE

#include "memory.h"

#include <stdio.h>
#include <sys/mman.h>

int memory_allocate(memory_region_t *region,
                    size_t size,
                    int protection)
{
    if (region == NULL || size == 0) {
        fprintf(stderr, "Invalid memory allocation arguments\n");
        return -1;
    }

    region->address = NULL;
    region->size = 0;

    void *address = mmap(NULL,
                         size,
                         protection,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1,
                         0);

    if (address == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    region->address = address;
    region->size = size;

    return 0;
}

int memory_set_protection(memory_region_t *region,
                          int protection)
{
    if (region == NULL ||
        region->address == NULL ||
        region->size == 0) {
        fprintf(stderr, "Invalid memory region\n");
        return -1;
    }

    if (mprotect(region->address,
                 region->size,
                 protection) != 0) {
        perror("mprotect");
        return -1;
    }

    return 0;
}

int memory_release(memory_region_t *region)
{
    if (region == NULL) {
        fprintf(stderr, "Invalid memory region\n");
        return -1;
    }

    if (region->address == NULL || region->size == 0)
        return 0;

    if (munmap(region->address, region->size) != 0) {
        perror("munmap");
        return -1;
    }

    region->address = NULL;
    region->size = 0;

    return 0;
}
