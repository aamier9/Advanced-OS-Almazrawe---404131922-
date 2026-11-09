
#include "pager.h"

#define PREFETCH_LIMIT 2

int hpager_handle_page_fault(pte_t *pt, size_t max_pgs,
                             uint32_t vpn)
{
    if (pt == NULL || vpn >= max_pgs)
        return -1;

    uint32_t main_frame = vpn + 200;

    /* Map the requested page */
    pt[vpn].frame_no = main_frame;
    pt[vpn].present = 1;
    pt[vpn].dirty = 0;

    /* Prefetch the next two pages */
    for (uint32_t i = 1; i <= PREFETCH_LIMIT; i++) {
        uint32_t next_vpn = vpn + i;

        if (next_vpn < max_pgs && !pt[next_vpn].present) {
            pt[next_vpn].frame_no = next_vpn + 200;
            pt[next_vpn].present = 1;
            pt[next_vpn].dirty = 0;
        }
    }

    return (int)main_frame;
}
