#include "memory.h"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size <= 0) {
        fprintf(stderr, "Could not determine page size\n");
        return 1;
    }

    memory_region_t region = {0};

    if (memory_allocate(&region,
                        (size_t)page_size,
                        PROT_READ | PROT_WRITE) != 0) {
        return 1;
    }

    strcpy((char *)region.address, "Memory Management PASS");

    if (strcmp((char *)region.address,
               "Memory Management PASS") != 0) {
        fprintf(stderr, "Memory read/write test failed\n");
        memory_release(&region);
        return 1;
    }

    if (memory_set_protection(&region, PROT_READ) != 0) {
        memory_release(&region);
        return 1;
    }

    printf("PASS: Memory allocation\n");
    printf("PASS: Memory read/write\n");
    printf("PASS: Memory protection\n");

    if (memory_release(&region) != 0)
        return 1;

    printf("PASS: Memory release\n");

    return 0;
}
