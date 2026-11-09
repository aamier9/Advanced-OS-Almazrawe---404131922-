#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint32_t frame_no;
    unsigned char present;
    unsigned char dirty;
} pte_t;

int apager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn) {
    if (!pt || vpn >= max_pgs) return -1;
    uint32_t frm = vpn + 100; 
    pt[vpn].frame_no = frm;
    pt[vpn].present = 1;
    pt[vpn].dirty = 0;
    return (int)frm;
}