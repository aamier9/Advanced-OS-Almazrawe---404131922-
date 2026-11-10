
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "elf_loader.h"

#define ELF_TARGET "tests/elf_target"

static int fail_test(const char *message,
                     FILE *file,
                     elf_image_t *image)
{
    fprintf(stderr, "FAIL: %s\n", message);

    if (file != NULL)
        fclose(file);

    elf_unload_image(image);

    return 1;
}

int main(void)
{
    elf_image_t image = {0};
    FILE *file = NULL;

    unsigned int load_count = 0;
    unsigned int bss_count = 0;

    printf("====================================\n");
    printf("       ELF MEMORY LOADER TEST       \n");
    printf("====================================\n\n");

    /* Step 1: Load ELF image. */
    printf("[1] Loading ELF image...\n");

    if (elf_load_image(ELF_TARGET, &image) != 0) {
        fprintf(stderr, "FAIL: ELF loading failed\n");
        return 1;
    }

    printf("PASS: ELF image loaded\n\n");

    /* Step 2: Validate allocated memory. */
    printf("[2] Checking allocated memory...\n");

    if (image.memory == NULL || image.size == 0) {
        return fail_test(
            "Invalid memory image",
            NULL,
            &image
        );
    }

    printf("PASS: Memory allocated\n");
    printf("Memory address: %p\n", image.memory);
    printf("Image size: %zu bytes\n\n", image.size);

    /* Step 3: Validate entry point. */
    printf("[3] Checking entry point...\n");

    if (image.entry_point < image.min_vaddr ||
        image.entry_point - image.min_vaddr >= image.size) {

        return fail_test(
            "Entry point outside memory image",
            NULL,
            &image
        );
    }

    printf("PASS: Entry point is inside image\n");
    printf("Entry point: 0x%lx\n",
           (unsigned long)image.entry_point);
    printf("Virtual base: 0x%lx\n\n",
           (unsigned long)image.min_vaddr);

    /* Step 4: Open original ELF file. */
    file = fopen(ELF_TARGET, "rb");

    if (file == NULL) {
        perror("fopen");
        return fail_test(
            "Cannot open original ELF file",
            NULL,
            &image
        );
    }

    Elf64_Ehdr ehdr;

    if (fread(&ehdr, sizeof(ehdr), 1, file) != 1) {
        return fail_test(
            "Cannot read ELF header",
            file,
            &image
        );
    }

    printf("[4] Verifying LOAD segments and BSS...\n\n");

    /* Step 5: Inspect every program header. */
    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {

        uint64_t phdr_offset =
            ehdr.e_phoff +
            (uint64_t)i * ehdr.e_phentsize;

        if (phdr_offset > LONG_MAX ||
            fseek(file, (long)phdr_offset, SEEK_SET) != 0) {

            return fail_test(
                "Cannot seek to program header",
                file,
                &image
            );
        }

        Elf64_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, file) != 1) {
            return fail_test(
                "Cannot read program header",
                file,
                &image
            );
        }

        /* Ignore non-LOAD segments. */
        if (phdr.p_type != PT_LOAD)
            continue;

        printf("LOAD segment %u:\n", load_count + 1);

        /* Validate segment memory range. */
        if (phdr.p_vaddr < image.min_vaddr) {
            return fail_test(
                "Segment starts before image base",
                file,
                &image
            );
        }

        uint64_t relative =
            phdr.p_vaddr - image.min_vaddr;

        if (relative > image.size ||
            phdr.p_memsz > image.size - relative ||
            relative > SIZE_MAX ||
            phdr.p_filesz > phdr.p_memsz ||
            phdr.p_filesz > SIZE_MAX) {

            return fail_test(
                "Invalid segment memory range",
                file,
                &image
            );
        }

        unsigned char *loaded =
            (unsigned char *)image.memory +
            (size_t)relative;

        /* Verify file-backed segment data. */
        if (phdr.p_filesz > 0) {

            if (phdr.p_offset > LONG_MAX) {
                return fail_test(
                    "Invalid segment file offset",
                    file,
                    &image
                );
            }

            unsigned char *expected =
                malloc((size_t)phdr.p_filesz);

            if (expected == NULL) {
                return fail_test(
                    "Memory allocation failed",
                    file,
                    &image
                );
            }

            if (fseek(file, (long)phdr.p_offset, SEEK_SET) != 0 ||
                fread(expected, 1,
                      (size_t)phdr.p_filesz, file)
                    != (size_t)phdr.p_filesz) {

                free(expected);

                return fail_test(
                    "Cannot read original segment data",
                    file,
                    &image
                );
            }

            if (memcmp(loaded, expected,
                       (size_t)phdr.p_filesz) != 0) {

                free(expected);

                return fail_test(
                    "Segment data mismatch",
                    file,
                    &image
                );
            }

            free(expected);

            printf("  PASS: File data matches\n");
        }

        /* Verify zero-filled memory (BSS). */
        if (phdr.p_memsz > phdr.p_filesz) {

            size_t zero_size =
                (size_t)(phdr.p_memsz - phdr.p_filesz);

            unsigned char *bss =
                loaded + (size_t)phdr.p_filesz;

            for (size_t j = 0; j < zero_size; j++) {

                if (bss[j] != 0) {
                    return fail_test(
                        "BSS contains non-zero data",
                        file,
                        &image
                    );
                }
            }

            bss_count++;

            printf("  PASS: BSS zero-filled (%zu bytes)\n",
                   zero_size);
        }

        load_count++;

        printf("  PASS: LOAD segment verified\n\n");
    }

    /* Step 6: Check that LOAD segments exist. */
    if (load_count == 0) {
        return fail_test(
            "No LOAD segments found",
            file,
            &image
        );
    }

    fclose(file);
    file = NULL;

    printf("====================================\n");
    printf("          TEST SUMMARY              \n");
    printf("====================================\n");

    printf("LOAD segments verified: %u\n", load_count);
    printf("BSS regions verified:   %u\n", bss_count);

    printf("PASS: All LOAD segments verified\n");

    /* Step 7: Release allocated memory. */
    printf("\n[5] Releasing memory...\n");

    elf_unload_image(&image);

    if (image.memory != NULL ||
        image.size != 0 ||
        image.min_vaddr != 0 ||
        image.entry_point != 0) {

        return fail_test(
            "Memory image was not reset",
            NULL,
            &image
        );
    }

    printf("PASS: Memory released successfully\n");

    printf("\n====================================\n");
    printf("    ALL ELF LOADER TESTS PASSED     \n");
    printf("====================================\n");

    return 0;
}
