
#include "demand_pager.h"

#include <stdio.h>
#include <stdint.h>

int main(void)
{
    demand_pager_t pager;

    if (demand_pager_init(&pager, 4 * 4096) != 0) {
        perror("demand_pager_init");
        return 1;
    }

    volatile unsigned char *memory =
        (volatile unsigned char *)pager.address;

    if (demand_pager_loaded_count(&pager) != 0) {
        printf("FAIL: Pages should not be loaded initially\n");
        demand_pager_destroy(&pager);
        return 1;
    }

    printf("PASS: No pages loaded initially\n");

    memory[0] = 0x11;

    if (demand_pager_loaded_count(&pager) != 1) {
        printf("FAIL: First page was not loaded\n");
        demand_pager_destroy(&pager);
        return 1;
    }

    printf("PASS: First page loaded on demand\n");

    memory[4096] = 0x22;

    if (demand_pager_loaded_count(&pager) != 2) {
        printf("FAIL: Second page was not loaded\n");
        demand_pager_destroy(&pager);
        return 1;
    }

    printf("PASS: Second page loaded on demand\n");

    if (pager.page_faults != 2) {
        printf("FAIL: Incorrect page fault count\n");
        demand_pager_destroy(&pager);
        return 1;
    }

    printf("PASS: Page fault counter\n");

    demand_pager_destroy(&pager);

    printf("PASS: Demand pager cleanup\n");

    return 0;
}
