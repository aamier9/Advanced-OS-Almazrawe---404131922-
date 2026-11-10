
#include "elf_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main(void)
{
    elf_image_t image = {0};

    printf("====================================\n");
    printf("     ELF MEMORY PROTECTION TEST\n");
    printf("====================================\n");

    /* Step 1: Load ELF image. */
    printf("\n[1] Loading ELF image...\n");

    if (elf_load_image("tests/elf_target", &image) != 0) {
        fprintf(stderr, "FAIL: ELF loading failed\n");
        return 1;
    }

    printf("PASS: ELF image loaded\n");

    /*
     * The first LOAD segment is read-only.
     * Its virtual address is 0x400000.
     */
    uint64_t read_only_vaddr = 0x400000;

    if (read_only_vaddr < image.min_vaddr ||
        read_only_vaddr - image.min_vaddr >= image.size) {
        fprintf(stderr, "FAIL: Invalid read-only address\n");
        elf_unload_image(&image);
        return 1;
    }

    unsigned char *target =
        (unsigned char *)image.memory +
        (size_t)(read_only_vaddr - image.min_vaddr);

    printf("\n[2] Testing write protection...\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        elf_unload_image(&image);
        return 1;
    }

    if (pid == 0) {
        /*
         * Child process:
         * Attempt to write to a read-only page.
         */
        *target = 0xAA;

        /* Reached only if protection did not stop the write. */
        _exit(0);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        elf_unload_image(&image);
        return 1;
    }

    if (WIFSIGNALED(status) &&
        WTERMSIG(status) == SIGSEGV) {

        printf("PASS: Write to read-only page blocked (SIGSEGV)\n");

    } else {
        printf("FAIL: Write protection test did not behave as expected\n");

        if (WIFEXITED(status))
            printf("Child exit status: %d\n", WEXITSTATUS(status));

        elf_unload_image(&image);
        return 1;
    }
        /*
     * Step 3: Test execution protection.
     * The page at 0x400000 has R-- permissions.
     */
    printf("\n[3] Testing execution protection...\n");

    pid = fork();

    if (pid < 0) {
        perror("fork");
        elf_unload_image(&image);
        return 1;
    }

    if (pid == 0) {
        /*
         * Attempt to execute instructions from a non-executable page.
         */
        uintptr_t address = (uintptr_t)target;

        void (*execute_target)(void) =
            (void (*)(void))address;

        execute_target();

        /* Reached only if execution was not blocked. */
        _exit(0);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        elf_unload_image(&image);
        return 1;
    }

    if (WIFSIGNALED(status) &&
        WTERMSIG(status) == SIGSEGV) {

        printf("PASS: Execution from non-executable page blocked (SIGSEGV)");

    } else {
        printf("FAIL: Execution protection test did not behave as expected\n");
        elf_unload_image(&image);
        return 1;
    }

    /* Step 3: Release image. */
    printf("\n[3] Releasing memory...\n");

    elf_unload_image(&image);

    if (image.memory == NULL && image.size == 0) {
        printf("PASS: Memory released successfully\n");
    } else {
        printf("FAIL: Memory release failed\n");
        return 1;
    }

    printf("\n====================================\n");
    printf(" ALL PROTECTION TESTS PASSED\n");
    printf("====================================\n");

    return 0;
}
