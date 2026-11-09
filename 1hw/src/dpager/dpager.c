#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint32_t frame_no;
    unsigned char present;
    uint32_t hits;
} pte_t;

int dpager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn) {
    if (!pt || vpn >= max_pgs) return -1;
    uint32_t f_id = vpn + 300;
    pt[vpn].frame_no = f_id;
    pt[vpn].present = 1;
    pt[vpn].hits++;
    return (int)f_id;
}