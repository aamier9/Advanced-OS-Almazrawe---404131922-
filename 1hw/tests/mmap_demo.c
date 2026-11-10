
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size <= 0) {
        perror("sysconf");
        return 1;
    }

    void *memory = mmap(
        NULL,
        (size_t)page_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    printf("Page size: %ld bytes\n", page_size);
    printf("Mapped address: %p\n", memory);

    strcpy((char *)memory, "Hello from mmap!");

    printf("Stored data: %s\n", (char *)memory);

    if (munmap(memory, (size_t)page_size) != 0) {
        perror("munmap");
        return 1;
    }

    printf("Memory unmapped successfully\n");

    return 0;
}