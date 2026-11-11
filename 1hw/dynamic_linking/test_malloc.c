#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const size_t size = 64;
    unsigned char *buffer = malloc(size);

    if (buffer == NULL) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    for (size_t i = 0; i < size; i++) {
        if (buffer[i] != 0xCC) {
            fprintf(stderr,
                    "FAIL: byte %zu has value 0x%02X\n",
                    i, buffer[i]);
            free(buffer);
            return 1;
        }
    }

    printf("PASS: all %zu bytes contain 0xCC\n", size);

    free(buffer);
    return 0;
}
