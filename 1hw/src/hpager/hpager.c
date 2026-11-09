#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PREFETCH_LIMIT 2

typedef struct {
    uint32_t frame_no;
    unsigned char present;
    unsigned char dirty;
} pte_t;

int hpager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn) {
    if (!pt || vpn >= max_pgs) return -1;
    uint32_t main_frm = vpn + 200;
    pt[vpn].frame_no = main_frm;
    pt[vpn].present = 1;

    for (int i = 1; i <= PREFETCH_LIMIT; i++) {
        uint32_t next_vpn = vpn + i;
        if (next_vpn < max_pgs && !pt[next_vpn].present) {
            pt[next_vpn].frame_no = next_vpn + 200;
            pt[next_vpn].present = 1;
        }
    }
    return (int)main_frm;
}