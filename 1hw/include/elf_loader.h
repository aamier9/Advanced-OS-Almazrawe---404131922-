
#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <elf.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *memory;
    size_t size;
    uint64_t min_vaddr;
    uint64_t entry_point;
} elf_image_t;

int elf_validate(const char *filename);
int elf_validate_segments(const char *filename);
int elf_print_segments(const char *filename);

/* Load ELF segments into a separate memory image. */
int elf_load_image(const char *filename, elf_image_t *image);

/* Release the allocated ELF image. */
void elf_unload_image(elf_image_t *image);

#endif
