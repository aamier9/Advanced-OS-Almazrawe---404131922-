#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Define basic PTE structure matching pagers
typedef struct {
    uint32_t frame_no;
    unsigned char present;
    unsigned char dirty;
    uint32_t hits;
} pte_t;

// Declare pager functions from respective modules
int apager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn);
int hpager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn);
int dpager_handle_page_fault(pte_t *pt, size_t max_pgs, uint32_t vpn);

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Starting Advanced OS Pager Tests ===\n\n");

    const size_t MAX_PAGES = 16;
    pte_t *page_table = (pte_t *)calloc(MAX_PAGES, sizeof(pte_t));
    if (!page_table) {
        perror("allocation failed");
        return 1;
    }

    // 1. Test Demand Pager (apager)
    printf("--- Testing Demand Pager (apager) ---\n");
    int frame1 = apager_handle_page_fault(page_table, MAX_PAGES, 2);
    printf("APAGER Result -> VPN 2 mapped to Frame: %d\n\n", frame1);

    // 2. Test Heuristic/Prefetching Pager (hpager)
    printf("--- Testing Heuristic Pager (hpager) ---\n");
    int frame2 = hpager_handle_page_fault(page_table, MAX_PAGES, 5);
    printf("HPAGER Result -> VPN 5 mapped to Frame: %d (with prefetch)\n", frame2);
    printf("Check prefetch status for VPN 6: present = %u\n\n", page_table[6].present);

    // 3. Test Dynamic Pager (dpager)
    printf("--- Testing Dynamic Pager (dpager) ---\n");
    int frame3 = dpager_handle_page_fault(page_table, MAX_PAGES, 10);
    printf("DPAGER Result -> VPN 10 mapped to Frame: %d\n", frame3);

    free(page_table);
    printf("\n=== All Pager Tests Completed Successfully ===\n");
    return 0;
}