#ifndef STACK_H
#define STACK_H

#include "memory.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    memory_region_t region;
    void *stack_base;
    size_t stack_size;
    uintptr_t stack_pointer;
} user_stack_t;

int stack_build(user_stack_t *stack,
                size_t stack_size,
                int argc,
                char *const argv[],
                char *const envp[],
                uintptr_t entry_point);

void stack_destroy(user_stack_t *stack);

#endif
