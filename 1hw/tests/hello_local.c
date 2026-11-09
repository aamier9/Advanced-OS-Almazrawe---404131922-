
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "pager.h"

#define MAX_PAGES 16

/* Stop the test immediately if a condition fails. */
#define CHECK(condition) do {                                      \
    if (!(condition)) {                                            \
        fprintf(stderr, "TEST FAILED: %s (line %d)\n",             \
                #condition, __LINE__);                             \
        exit(EXIT_FAILURE);                                        \
    }                                                              \
} while (0)

/* Check whether a PTE still contains its expected values. */
static int entry_matches(const pte_t *entry,
                         uint32_t frame,
                         unsigned char present,
                         unsigned char dirty,
                         uint32_t hits)
{
    return entry->frame_no == frame &&
           entry->present == present &&
           entry->dirty == dirty &&
           entry->hits == hits;
}

static void test_apager(void)
{
    pte_t *pt = calloc(MAX_PAGES, sizeof(pte_t));
    CHECK(pt != NULL);

    int frame = apager_handle_page_fault(pt, MAX_PAGES, 2);

    CHECK(frame == 102);
    CHECK(entry_matches(&pt[2], 102, 1, 0, 0));

    /* Invalid arguments must be rejected. */
    CHECK(apager_handle_page_fault(NULL, MAX_PAGES, 2) == -1);
    CHECK(apager_handle_page_fault(pt, MAX_PAGES, MAX_PAGES) == -1);

    free(pt);
    printf("APAGER: PASS\n");
}

static void test_hpager(void)
{
    pte_t *pt = calloc(MAX_PAGES, sizeof(pte_t));
    CHECK(pt != NULL);

    int frame = hpager_handle_page_fault(pt, MAX_PAGES, 5);

    CHECK(frame == 205);
    CHECK(entry_matches(&pt[5], 205, 1, 0, 0));
    CHECK(entry_matches(&pt[6], 206, 1, 0, 0));
    CHECK(entry_matches(&pt[7], 207, 1, 0, 0));

    /* An already-present page must not be overwritten by prefetch. */
    pt[9].frame_no = 999;
    pt[9].present = 1;
    pt[9].dirty = 1;
    pt[9].hits = 7;

    CHECK(hpager_handle_page_fault(pt, MAX_PAGES, 8) == 208);
    CHECK(entry_matches(&pt[9], 999, 1, 1, 7));

    /* Test the last valid VPN: prefetch must stay within the table. */
    CHECK(hpager_handle_page_fault(pt, MAX_PAGES, 15) == 215);
    CHECK(pt[15].present == 1);

    CHECK(hpager_handle_page_fault(NULL, MAX_PAGES, 5) == -1);
    CHECK(hpager_handle_page_fault(pt, MAX_PAGES, MAX_PAGES) == -1);

    free(pt);
    printf("HPAGER: PASS\n");
}

static void test_dpager(void)
{
    pte_t *pt = calloc(MAX_PAGES, sizeof(pte_t));
    CHECK(pt != NULL);

    int frame = dpager_handle_page_fault(pt, MAX_PAGES, 10);

    CHECK(frame == 310);
    CHECK(entry_matches(&pt[10], 310, 1, 0, 1));

    CHECK(dpager_handle_page_fault(NULL, MAX_PAGES, 10) == -1);
    CHECK(dpager_handle_page_fault(pt, MAX_PAGES, MAX_PAGES) == -1);

    free(pt);
    printf("DPAGER: PASS\n");
}

int main(void)
{
    printf("=== Advanced OS Pager Tests ===\n");

    test_apager();
    test_hpager();
    test_dpager();

    printf("=== All Pager Tests Passed ===\n");
    return EXIT_SUCCESS;
}
