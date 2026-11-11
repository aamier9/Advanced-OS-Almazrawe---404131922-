
#include "hybrid_pager.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define TEST_ELF "tests/elf_target"

/*
 * These addresses are based on the current tests/elf_target
 * program headers shown by readelf -l.
 *
 * LOAD 3:
 *   p_vaddr  = 0x4ac108
 *   p_filesz = 0x5940
 *   p_memsz  = 0xb160
 *
 * BSS starts at 0x4b1a48.
 * The first BSS-only page starts at 0x4b2000.
 */
#define FIRST_BSS_ONLY_PAGE 0x4b2000ULL

static int check(int condition, const char *message)
{
    if (!condition) {
        printf("FAIL: %s\n", message);
        return -1;
    }

    printf("PASS: %s\n", message);
    return 0;
}

int main(void)
{
    hybrid_pager_t pager;

    printf("====================================\n");
    printf("       HYBRID PAGING TEST\n");
    printf("====================================\n");

    if (hybrid_pager_load(TEST_ELF, &pager) != 0) {
        perror("hybrid_pager_load");
        return EXIT_FAILURE;
    }

    int failures = 0;

    printf("\n[1] Checking initial preloading...\n");

    if (check(
            pager.preloaded_pages > 0,
            "Initialized ELF pages preloaded") != 0) {
        failures++;
    }

    if (check(
            hybrid_pager_loaded_count(&pager) ==
                pager.preloaded_pages,
            "Only preloaded pages are initially accessible") != 0) {
        failures++;
    }

    printf("\n[2] Checking BSS demand loading...\n");

    if (FIRST_BSS_ONLY_PAGE < pager.min_vaddr) {
        printf("FAIL: BSS test address is below image base\n");
        failures++;
    } else {

        uint64_t relative =
            FIRST_BSS_ONLY_PAGE - pager.min_vaddr;

        size_t page_index =
            (size_t)(relative / pager.page_size);

        if (page_index >= pager.page_count ||
            pager.page_states[page_index] != 0) {

            printf("FAIL: Expected BSS page is not initially unloaded\n");
            failures++;

        } else {

            printf("PASS: BSS page is initially unloaded\n");

            volatile unsigned char *bss_byte =
                (volatile unsigned char *)pager.address +
                (size_t)relative;

            unsigned char value = *bss_byte;

            if (check(
                    value == 0,
                    "BSS page is zero-initialized") != 0) {
                failures++;
            }

            if (check(
                    pager.page_states[page_index] == 2,
                    "BSS page loaded on demand") != 0) {
                failures++;
            }
        }
    }

    printf("\n[3] Checking prefetch...\n");

    if (check(
            pager.prefetched_pages >= 2,
            "Next two pages prefetched") != 0) {
        failures++;
    }

    printf("\n[4] Checking statistics...\n");

    if (check(
            pager.page_faults >= 1,
            "Page fault counter updated") != 0) {
        failures++;
    }

    if (check(
            pager.demand_pages >= 1,
            "Demand page counter updated") != 0) {
        failures++;
    }

    printf("\n====================================\n");
    printf("       HYBRID PAGING STATISTICS\n");
    printf("====================================\n");

    printf("Page size       : %zu bytes\n", pager.page_size);
    printf("Total pages     : %zu\n", pager.page_count);
    printf("Preloaded pages : %zu\n", pager.preloaded_pages);
    printf("Demand pages    : %d\n", (int)pager.demand_pages);
    printf("Prefetched pages: %d\n", (int)pager.prefetched_pages);
    printf("Page faults     : %d\n", (int)pager.page_faults);
    printf("Loaded pages    : %zu\n",
           hybrid_pager_loaded_count(&pager));

    printf("\n[5] Cleanup...\n");

    hybrid_pager_destroy(&pager);

    printf("PASS: Hybrid Pager cleanup\n");

    printf("\n====================================\n");

    if (failures == 0) {
        printf("ALL HYBRID PAGING TESTS PASSED\n");
        return EXIT_SUCCESS;
    }

    printf("HYBRID PAGING TESTS FAILED: %d\n", failures);
    return EXIT_FAILURE;
}
