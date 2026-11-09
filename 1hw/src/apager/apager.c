
#include "pager.h"

int apager_handle_page_fault(pte_t *pt, size_t max_pgs,
                             uint32_t vpn)
{
    if (pt == NULL || vpn >= max_pgs)
        return -1;

    uint32_t frame = vpn + 100;

    pt[vpn].frame_no = frame;
    pt[vpn].present = 1;
    pt[vpn].dirty = 0;

    return (int)frame;
}
