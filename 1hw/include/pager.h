
#ifndef PAGER_H
#define PAGER_H

#include <stdint.h>
#include <stddef.h>

/* Common Page Table Entry */
typedef struct {
    uint32_t frame_no;
    unsigned char present;
    unsigned char dirty;
    uint32_t hits;
} pte_t;

/* Pager function declarations */
int apager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn);

int hpager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn);

int dpager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn);

#endif
