#define _GNU_SOURCE

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static void *(*real_malloc)(size_t) = NULL;
static __thread int resolving = 0;

static void write_text(const char *text)
{
    write(STDERR_FILENO, text, strlen(text));
}

static void write_number(size_t value)
{
    char buffer[32];
    size_t i = 0;

    do {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    } while (value != 0 && i < sizeof(buffer));

    while (i > 0)
        write(STDERR_FILENO, &buffer[--i], 1);
}

static void write_address(const void *ptr)
{
    static const char hex[] = "0123456789abcdef";
    uintptr_t value = (uintptr_t)ptr;
    char buffer[2 * sizeof(value) + 1];
    size_t i = sizeof(buffer);

    buffer[--i] = '\0';

    do {
        buffer[--i] = hex[value & 0xF];
        value >>= 4;
    } while (value != 0 && i > 0);

    write_text("0x");
    write(STDERR_FILENO, buffer + i, sizeof(buffer) - i - 1);
}

void *malloc(size_t size)
{
    if (real_malloc == NULL) {
        if (resolving)
            return NULL;

        resolving = 1;
        void *symbol = dlsym(RTLD_NEXT, "malloc");
        resolving = 0;

        if (symbol == NULL)
            return NULL;

        memcpy(&real_malloc, &symbol, sizeof(real_malloc));
    }

    void *ptr = real_malloc(size);

    write_text("[malloc_hook] Requested size: ");
    write_number(size);
    write_text(" bytes, returned address: ");
    write_address(ptr);
    write_text("\n");

    if (ptr != NULL)
        memset(ptr, 0xCC, size);

    return ptr;
}
