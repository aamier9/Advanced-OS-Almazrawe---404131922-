#define _DEFAULT_SOURCE

#include "stack.h"

#include <elf.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct {
    uint64_t type;
    uint64_t value;
} stack_auxv_t;

static uintptr_t align_down(uintptr_t address, size_t alignment)
{
    return address - (address % alignment);
}

static int copy_strings(unsigned char **cursor,
                        char *const strings[],
                        size_t count,
                        uintptr_t addresses[])
{
    for (size_t i = count; i > 0; i--) {
        size_t length = strlen(strings[i - 1]) + 1;

        if ((uintptr_t)*cursor < length)
            return -1;

        *cursor -= length;
        memcpy(*cursor, strings[i - 1], length);
        addresses[i - 1] = (uintptr_t)*cursor;
    }

    return 0;
}

int stack_build(user_stack_t *stack,
                size_t stack_size,
                int argc,
                char *const argv[],
                char *const envp[],
                uintptr_t entry_point)
{
    if (stack == NULL || stack_size == 0 || argc < 0 ||
        (argc > 0 && argv == NULL)) {
        fprintf(stderr, "Invalid stack arguments\n");
        return -1;
    }

    stack->region.address = NULL;
    stack->region.size = 0;
    stack->stack_base = NULL;
    stack->stack_size = 0;
    stack->stack_pointer = 0;

    size_t envc = 0;

    if (envp != NULL) {
        while (envp[envc] != NULL)
            envc++;
    }

    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size <= 0) {
        fprintf(stderr, "Could not determine page size\n");
        return -1;
    }

    size_t guard_size = (size_t)page_size;

    if (stack_size % guard_size != 0 ||
        stack_size > SIZE_MAX - guard_size) {
        fprintf(stderr, "Invalid stack size\n");
        return -1;
    }

    size_t total_size = stack_size + guard_size;

    if (memory_allocate(&stack->region,
                        total_size,
                        PROT_READ | PROT_WRITE) != 0) {
        return -1;
    }

    if (memory_set_protection(
            &(memory_region_t){
                .address = stack->region.address,
                .size = guard_size
            },
            PROT_NONE) != 0) {
        memory_release(&stack->region);
        return -1;
    }

    unsigned char *base =
        (unsigned char *)stack->region.address + guard_size;

    unsigned char *cursor = base + stack_size;

    uintptr_t *argv_addresses = NULL;
    uintptr_t *env_addresses = NULL;

    if (argc > 0) {
        argv_addresses = __builtin_alloca(
            (size_t)argc * sizeof(uintptr_t));
    }

    if (envc > 0) {
        env_addresses = __builtin_alloca(
            envc * sizeof(uintptr_t));
    }

    if (copy_strings(&cursor, argv, (size_t)argc,
                     argv_addresses) != 0 ||
        copy_strings(&cursor, envp, envc,
                     env_addresses) != 0) {
        fprintf(stderr, "Stack strings exceed available space\n");
        memory_release(&stack->region);
        return -1;
    }

    size_t aux_count = 3;
    size_t word_count =
        1 + (size_t)argc + 1 +
        envc + 1 + aux_count * 2;

    if (word_count > (size_t)(cursor - base) / sizeof(uintptr_t)) {
        fprintf(stderr, "Not enough space for initial stack\n");
        memory_release(&stack->region);
        return -1;
    }

    uintptr_t table_address = align_down(
        (uintptr_t)cursor - word_count * sizeof(uintptr_t),
        16);

    if (table_address < (uintptr_t)base) {
        fprintf(stderr, "Stack table exceeds available space\n");
        memory_release(&stack->region);
        return -1;
    }

    uintptr_t *table = (uintptr_t *)table_address;
    size_t index = 0;

    table[index++] = (uintptr_t)argc;

    for (int i = 0; i < argc; i++)
        table[index++] = argv_addresses[i];

    table[index++] = 0;

    for (size_t i = 0; i < envc; i++)
        table[index++] = env_addresses[i];

    table[index++] = 0;

    stack_auxv_t *auxv = (stack_auxv_t *)&table[index];

    auxv[0].type = AT_PAGESZ;
    auxv[0].value = (uint64_t)page_size;

    auxv[1].type = AT_ENTRY;
    auxv[1].value = entry_point;

    auxv[2].type = AT_NULL;
    auxv[2].value = 0;

    stack->stack_base = base;
    stack->stack_size = stack_size;
    stack->stack_pointer = table_address;

    return 0;
}

void stack_destroy(user_stack_t *stack)
{
    if (stack == NULL)
        return;

    memory_release(&stack->region);

    stack->stack_base = NULL;
    stack->stack_size = 0;
    stack->stack_pointer = 0;
}
