#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

typedef struct {
    void *address;
    size_t size;
} memory_region_t;

int memory_allocate(memory_region_t *region,
                    size_t size,
                    int protection);

int memory_set_protection(memory_region_t *region,
                          int protection);

int memory_release(memory_region_t *region);

#endif
