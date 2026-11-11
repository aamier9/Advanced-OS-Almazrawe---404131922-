#include <string.h>
#include "stack.h"

#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    char *args[] = {
        "target_program",
        "input.txt",
        NULL
    };

    char *env[] = {
        "MODE=test",
        "PATH=/usr/bin",
        NULL
    };

    user_stack_t stack;

    if (stack_build(&stack,
                    64 * 1024,
                    2,
                    args,
                    env,
                    0x401000) != 0) {
        return 1;
    }

    uintptr_t *words = (uintptr_t *)stack.stack_pointer;

    if (words[0] != 2) {
        fprintf(stderr, "FAIL: argc\n");
        stack_destroy(&stack);
        return 1;
    }

    char *arg0 = (char *)words[1];
    char *arg1 = (char *)words[2];

    if (strcmp(arg0, "target_program") != 0 ||
        strcmp(arg1, "input.txt") != 0) {
        fprintf(stderr, "FAIL: argv\n");
        stack_destroy(&stack);
        return 1;
    }

    if (words[3] != 0) {
        fprintf(stderr, "FAIL: argv terminator\n");
        stack_destroy(&stack);
        return 1;
    }

    char *env0 = (char *)words[4];
    char *env1 = (char *)words[5];

    if (strcmp(env0, "MODE=test") != 0 ||
        strcmp(env1, "PATH=/usr/bin") != 0 ||
        words[6] != 0) {
        fprintf(stderr, "FAIL: envp\n");
        stack_destroy(&stack);
        return 1;
    }

    Elf64_auxv_t *auxv = (Elf64_auxv_t *)&words[7];

    if (auxv[0].a_type != AT_PAGESZ ||
        auxv[1].a_type != AT_ENTRY ||
        auxv[1].a_un.a_val != 0x401000 ||
        auxv[2].a_type != AT_NULL) {
        fprintf(stderr, "FAIL: auxv\n");
        stack_destroy(&stack);
        return 1;
    }

    if (stack.stack_pointer % 16 != 0) {
        fprintf(stderr, "FAIL: stack alignment\n");
        stack_destroy(&stack);
        return 1;
    }

    printf("PASS: argc and argv\n");
    printf("PASS: envp\n");
    printf("PASS: auxiliary vector\n");
    printf("PASS: stack alignment\n");

    if (stack_destroy(&stack), stack.region.address != NULL) {
        fprintf(stderr, "FAIL: stack release\n");
        return 1;
    }

    printf("PASS: stack release\n");

    return 0;
}
