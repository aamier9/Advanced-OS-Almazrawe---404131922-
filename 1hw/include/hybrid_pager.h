
#ifndef HYBRID_PAGER_H
#define HYBRID_PAGER_H

#include <stddef.h>
#include <stdint.h>
#include <signal.h>

typedef struct {
    void *address;
    size_t size;
    size_t page_size;
    size_t page_count;

    uint64_t min_vaddr;
    uint64_t entry_point;

    volatile sig_atomic_t *page_states;
    volatile sig_atomic_t page_faults;
    volatile sig_atomic_t demand_pages;
    volatile sig_atomic_t prefetched_pages;

    size_t preloaded_pages;

    void *private_data;
    int active;
} hybrid_pager_t;

int hybrid_pager_load(
    const char *filename,
    hybrid_pager_t *pager
);

void hybrid_pager_destroy(hybrid_pager_t *pager);

size_t hybrid_pager_loaded_count(
    const hybrid_pager_t *pager
);

#endif
