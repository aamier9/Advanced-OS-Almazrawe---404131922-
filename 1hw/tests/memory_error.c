
#include "demand_pager.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>

int main(void)
{
    demand_pager_t pager;

    if (demand_pager_init(&pager, 4 * 4096) != 0) {
        perror("demand_pager_init");
        return 1;
    }

    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        demand_pager_destroy(&pager);
        return 1;
    }

    if (child == 0) {
        /*
         * This address is outside the managed region.
         * It must not be handled as a demand page fault.
         */
        volatile int *invalid_address =
            (volatile int *)(uintptr_t)0x1000;

        *invalid_address = 123;

        _exit(0);
    }

    int status;

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        demand_pager_destroy(&pager);
        return 1;
    }

    demand_pager_destroy(&pager);

if (WIFSIGNALED(status)) {
    printf("Child terminated by signal: %d\n",
           WTERMSIG(status));
} else if (WIFEXITED(status)) {
    printf("Child exited with status: %d\n",
           WEXITSTATUS(status));
} else {
    printf("Child ended in an unexpected state\n");
}

if (WIFSIGNALED(status) &&
    WTERMSIG(status) == SIGSEGV) {
    printf("PASS: Invalid memory access preserved SIGSEGV\n");
    return 0;
}

printf("FAIL: Invalid memory access was not reported as SIGSEGV\n");
return 1;
}
