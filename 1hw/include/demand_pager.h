
#ifndef DEMAND_PAGER_H
#define DEMAND_PAGER_H

#include <stddef.h>
#include <signal.h>

typedef struct {
    void *address;
    size_t size;
    size_t page_size;
    size_t page_count;
    volatile sig_atomic_t *loaded_pages;
    volatile sig_atomic_t page_faults;
    int active;
} demand_pager_t;

int demand_pager_init(demand_pager_t *pager, size_t size);

void demand_pager_destroy(demand_pager_t *pager);

size_t demand_pager_loaded_count(const demand_pager_t *pager);

#endif
