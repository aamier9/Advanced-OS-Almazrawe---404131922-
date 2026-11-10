
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

#include "elf_loader.h"

#define INVALID_ENTRY_FILE "tests/invalid_entry.elf"
#define OVERLAP_FILE       "tests/overlap.elf"

/* Copy an ELF file so the original remains unchanged. */
static int copy_file(const char *source, const char *destination)
{
    FILE *src = fopen(source, "rb");
    if (src == NULL)
        return -1;

    FILE *dst = fopen(destination, "wb");
    if (dst == NULL) {
        fclose(src);
        return -1;
    }

    char buffer[4096];
    size_t bytes;
    int result = 0;

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            result = -1;
            break;
        }
    }

    if (ferror(src))
        result = -1;

    fclose(src);
    fclose(dst);
    return result;
}

/* Change the ELF entry point in a copied file. */
static int make_invalid_entry(const char *filename)
{
    FILE *file = fopen(filename, "r+b");
    if (file == NULL)
        return -1;

    Elf64_Ehdr header;

    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return -1;
    }

    header.e_entry = 0x400100;

    if (fseek(file, 0, SEEK_SET) != 0 ||
        fwrite(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

/*
 * Move the first LOAD segment to the start of the second LOAD segment.
 * This creates overlapping memory ranges while preserving alignment
 * for the expected test ELF.
 */
static int make_overlapping_segments(const char *filename)
{
    FILE *file = fopen(filename, "r+b");
    if (file == NULL)
        return -1;

    Elf64_Ehdr header;

    if (fread(&header, sizeof(header), 1, file) != 1 ||
        header.e_phentsize != sizeof(Elf64_Phdr)) {
        fclose(file);
        return -1;
    }

    Elf64_Phdr first;
    Elf64_Phdr second;

    int found = 0;

    for (unsigned int i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        long offset = (long)(header.e_phoff +
                             (uint64_t)i * header.e_phentsize);

        if (fseek(file, offset, SEEK_SET) != 0 ||
            fread(&phdr, sizeof(phdr), 1, file) != 1) {
            fclose(file);
            return -1;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0)
            continue;

        if (found == 0) {
            first = phdr;
            found = 1;
        } else {
            second = phdr;
            found = 2;
            break;
        }
    }

    if (found < 2) {
        fclose(file);
        return -1;
    }

    /* Check that the modified first segment remains aligned. */
    if (first.p_align > 1 &&
        second.p_vaddr % first.p_align != first.p_offset % first.p_align) {
        fclose(file);
        return -1;
    }

    first.p_vaddr = second.p_vaddr;

    /* Find and update the original first LOAD program header. */
    for (unsigned int i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        long offset = (long)(header.e_phoff +
                             (uint64_t)i * header.e_phentsize);

        if (fseek(file, offset, SEEK_SET) != 0 ||
            fread(&phdr, sizeof(phdr), 1, file) != 1) {
            fclose(file);
            return -1;
        }

        if (phdr.p_type == PT_LOAD &&
            phdr.p_memsz != 0 &&
            phdr.p_vaddr != second.p_vaddr) {

            /* Identify the first LOAD by its original offset and size. */
            if (phdr.p_offset == first.p_offset &&
                phdr.p_memsz == first.p_memsz) {
                if (fseek(file, offset, SEEK_SET) != 0 ||
                    fwrite(&first, sizeof(first), 1, file) != 1) {
                    fclose(file);
                    return -1;
                }
                fclose(file);
                return 0;
            }
        }
    }

    fclose(file);
    return -1;
}

int main(void)
{
    printf("=== ELF Loader Validation Test ===\n\n");

    printf("Testing static ELF executable:\n");

    if (elf_validate("tests/elf_target") != 0) {
        printf("FAIL: static ELF rejected\n");
        return 1;
    }

    printf("PASS: static ELF accepted\n\n");

    printf("Testing PIE executable:\n");

    if (elf_validate("test_runner") == 0) {
        printf("FAIL: PIE executable unexpectedly accepted\n");
        return 1;
    }

    printf("PASS: PIE executable rejected as expected\n\n");

    printf("Testing LOAD segment validation:\n");

    if (elf_validate_segments("tests/elf_target") != 0) {
        printf("FAIL: LOAD segment validation failed\n");
        return 1;
    }

    printf("PASS: LOAD segments are valid\n\n");

    printf("Testing invalid entry point:\n");

    if (copy_file("tests/elf_target", INVALID_ENTRY_FILE) != 0 ||
        make_invalid_entry(INVALID_ENTRY_FILE) != 0) {
        printf("FAIL: Could not create invalid-entry test file\n");
        remove(INVALID_ENTRY_FILE);
        return 1;
    }

    if (elf_validate_segments(INVALID_ENTRY_FILE) == 0) {
        printf("FAIL: Invalid entry point unexpectedly accepted\n");
        remove(INVALID_ENTRY_FILE);
        return 1;
    }

    printf("PASS: Invalid entry point rejected\n\n");
    remove(INVALID_ENTRY_FILE);

    printf("Testing overlapping LOAD segments:\n");

    if (copy_file("tests/elf_target", OVERLAP_FILE) != 0 ||
        make_overlapping_segments(OVERLAP_FILE) != 0) {
        printf("FAIL: Could not create overlapping-segment test file\n");
        remove(OVERLAP_FILE);
        return 1;
    }

    if (elf_validate_segments(OVERLAP_FILE) == 0) {
        printf("FAIL: Overlapping LOAD segments unexpectedly accepted\n");
        remove(OVERLAP_FILE);
        return 1;
    }

    printf("PASS: Overlapping LOAD segments rejected\n\n");
    remove(OVERLAP_FILE);

    printf("Testing LOAD segment display:\n");

    if (elf_print_segments("tests/elf_target") != 0) {
        printf("FAIL: Could not read program segments\n");
        return 1;
    }

    printf("\nPASS: Program segments read successfully\n");

    printf("\n=== All ELF validation tests passed ===\n");

    return 0;
}
