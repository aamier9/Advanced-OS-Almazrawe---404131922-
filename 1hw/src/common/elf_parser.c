
#include "elf_loader.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

/*
 * Validate a 64-bit little-endian x86-64 ET_EXEC ELF file.
 *
 * Return:
 *   0  Success
 *  -1  Validation failure
 */
int elf_validate(const char *filename)
{
    if (filename == NULL) {
        fprintf(stderr, "Error: filename is NULL\n");
        return -1;
    }

    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        perror("Error opening ELF file");
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error seeking file\n");
        fclose(file);
        return -1;
    }

    long file_size = ftell(file);

    if (file_size < 0) {
        fprintf(stderr, "Error determining file size\n");
        fclose(file);
        return -1;
    }

    rewind(file);

    if ((uint64_t)file_size < sizeof(Elf64_Ehdr)) {
        fprintf(stderr, "Error: file is too small\n");
        fclose(file);
        return -1;
    }

    Elf64_Ehdr ehdr;

    if (fread(&ehdr, sizeof(ehdr), 1, file) != 1) {
        fprintf(stderr, "Error reading ELF header\n");
        fclose(file);
        return -1;
    }

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Error: invalid ELF magic\n");
        fclose(file);
        return -1;
    }

    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "Error: not an ELF64 file\n");
        fclose(file);
        return -1;
    }

    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) {
        fprintf(stderr, "Error: unsupported byte order\n");
        fclose(file);
        return -1;
    }

    if (ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
        ehdr.e_version != EV_CURRENT) {
        fprintf(stderr, "Error: unsupported ELF version\n");
        fclose(file);
        return -1;
    }

    if (ehdr.e_machine != EM_X86_64) {
        fprintf(stderr, "Error: unsupported architecture\n");
        fclose(file);
        return -1;
    }

    if (ehdr.e_type != ET_EXEC) {
        fprintf(stderr, "Error: only ET_EXEC is supported\n");
        fclose(file);
        return -1;
    }

    if (ehdr.e_ehsize != sizeof(Elf64_Ehdr) ||
        ehdr.e_phentsize != sizeof(Elf64_Phdr)) {
        fprintf(stderr, "Error: invalid ELF header sizes\n");
        fclose(file);
        return -1;
    }

    if (ehdr.e_phnum == 0 ||
        ehdr.e_phoff > (uint64_t)file_size) {
        fprintf(stderr, "Error: invalid program header table\n");
        fclose(file);
        return -1;
    }

    uint64_t remaining =
        (uint64_t)file_size - ehdr.e_phoff;

    if ((uint64_t)ehdr.e_phnum >
        remaining / ehdr.e_phentsize) {
        fprintf(stderr,
                "Error: program header table exceeds file size\n");
        fclose(file);
        return -1;
    }

    printf("ELF validation successful\n");
    printf("Entry point: 0x%lx\n",
           (unsigned long)ehdr.e_entry);
    printf("Program headers: %u\n",
           (unsigned int)ehdr.e_phnum);

    fclose(file);
    return 0;
}

/*
 * Validate all PT_LOAD segments.
 *
 * Checks:
 * 1. File size versus memory size.
 * 2. File boundaries.
 * 3. Virtual address overflow.
 * 4. Alignment.
 * 5. Supported segment flags.
 * 6. Entry point belongs to an executable segment.
 * 7. Memory-range overlap.
 *
 * LOAD segments are sorted by virtual address before
 * checking overlap. Program header order is not assumed.
 */
int elf_validate_segments(const char *filename)
{
    if (elf_validate(filename) != 0)
        return -1;

    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        perror("Error opening ELF file");
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    long file_size = ftell(file);

    if (file_size < 0) {
        fclose(file);
        return -1;
    }

    rewind(file);

    Elf64_Ehdr ehdr;

    if (fread(&ehdr, sizeof(ehdr), 1, file) != 1) {
        fprintf(stderr, "Error reading ELF header\n");
        fclose(file);
        return -1;
    }

    Elf64_Phdr *segments =
        calloc(ehdr.e_phnum, sizeof(Elf64_Phdr));

    if (segments == NULL) {
        fprintf(stderr, "Error allocating segment table\n");
        fclose(file);
        return -1;
    }

    unsigned int load_count = 0;

    /* Read and validate each program header. */
    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {

        uint64_t offset =
            ehdr.e_phoff +
            (uint64_t)i * ehdr.e_phentsize;

        if (offset > LONG_MAX ||
            fseek(file, (long)offset, SEEK_SET) != 0) {

            fprintf(stderr, "Error seeking program header\n");
            free(segments);
            fclose(file);
            return -1;
        }

        Elf64_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, file) != 1) {
            fprintf(stderr, "Error reading program header\n");
            free(segments);
            fclose(file);
            return -1;
        }

        if (phdr.p_type != PT_LOAD)
            continue;

        /* Check 1: File size must not exceed memory size. */
        if (phdr.p_filesz > phdr.p_memsz) {
            fprintf(stderr,
                    "Invalid LOAD: filesz > memsz\n");
            free(segments);
            fclose(file);
            return -1;
        }

        /* Check 2: File boundaries. */
        if (phdr.p_offset > (uint64_t)file_size ||
            phdr.p_filesz >
            (uint64_t)file_size - phdr.p_offset) {

            fprintf(stderr,
                    "Invalid LOAD: segment exceeds file bounds\n");
            free(segments);
            fclose(file);
            return -1;
        }

        /* Check 3: Virtual address overflow. */
        if (phdr.p_memsz >
            UINT64_MAX - phdr.p_vaddr) {

            fprintf(stderr,
                    "Invalid LOAD: virtual address overflow\n");
            free(segments);
            fclose(file);
            return -1;
        }

        /* Check 4: Alignment. */
        if (phdr.p_align > 1) {

            if ((phdr.p_align &
                 (phdr.p_align - 1)) != 0) {

                fprintf(stderr,
                        "Invalid LOAD: alignment is not a power of 2\n");
                free(segments);
                fclose(file);
                return -1;
            }

            if (phdr.p_vaddr % phdr.p_align !=
                phdr.p_offset % phdr.p_align) {

                fprintf(stderr,
                        "Invalid LOAD: alignment mismatch\n");
                free(segments);
                fclose(file);
                return -1;
            }
        }

        /* Check 5: Supported segment flags. */
        if (phdr.p_flags & ~(PF_R | PF_W | PF_X)) {
            fprintf(stderr,
                    "Invalid LOAD: unsupported permission flags\n");
            free(segments);
            fclose(file);
            return -1;
        }

        /* Reject writable and executable segments. */
        if ((phdr.p_flags & (PF_W | PF_X)) ==
            (PF_W | PF_X)) {

            fprintf(stderr,
                    "Invalid LOAD: writable-executable segment\n");
            free(segments);
            fclose(file);
            return -1;
        }

        segments[load_count++] = phdr;
    }

    fclose(file);

    if (load_count == 0) {
        fprintf(stderr, "Error: no LOAD segments found\n");
        free(segments);
        return -1;
    }

    /* Sort LOAD segments by virtual address. */
    for (unsigned int i = 0; i < load_count; i++) {

        for (unsigned int j = i + 1; j < load_count; j++) {

            if (segments[j].p_vaddr <
                segments[i].p_vaddr) {

                Elf64_Phdr temp = segments[i];
                segments[i] = segments[j];
                segments[j] = temp;
            }
        }
    }

    /*
     * Check 6: Entry point must belong to an executable LOAD segment.
     */
    int entry_is_executable = 0;

    for (unsigned int i = 0; i < load_count; i++) {

        uint64_t segment_start = segments[i].p_vaddr;
        uint64_t segment_end =
            segment_start + segments[i].p_memsz;

        if (segments[i].p_memsz > 0 &&
            (segments[i].p_flags & PF_X) &&
            ehdr.e_entry >= segment_start &&
            ehdr.e_entry < segment_end) {

            entry_is_executable = 1;
            break;
        }
    }

    if (!entry_is_executable) {
        fprintf(stderr,
                "Invalid ELF: entry point is not inside an executable LOAD segment\n");
        free(segments);
        return -1;
    }

    /*
     * Check 7: Memory-range overlap.
     *
     * Zero-sized segments are ignored.
     * Compare each non-empty segment with the previous
     * non-empty segment, not simply the previous array entry.
     */
    uint64_t previous_end = 0;
    int have_previous_segment = 0;

    for (unsigned int i = 0; i < load_count; i++) {

        if (segments[i].p_memsz == 0)
            continue;

        uint64_t current_start = segments[i].p_vaddr;
        uint64_t current_end =
            current_start + segments[i].p_memsz;

        if (have_previous_segment &&
            current_start < previous_end) {

            fprintf(stderr,
                    "Invalid ELF: overlapping LOAD segments\n");
            free(segments);
            return -1;
        }

        previous_end = current_end;
        have_previous_segment = 1;
    }

    free(segments);

    printf("All LOAD segments passed validation\n");
    return 0;
}

/*
 * Print all PT_LOAD program segments.
 */
int elf_print_segments(const char *filename)
{
    if (elf_validate(filename) != 0)
        return -1;

    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        perror("Error opening ELF file");
        return -1;
    }

    Elf64_Ehdr ehdr;

    if (fread(&ehdr, sizeof(ehdr), 1, file) != 1) {
        fprintf(stderr, "Error reading ELF header\n");
        fclose(file);
        return -1;
    }

    printf("\n=== ELF Program Segments ===\n");

    unsigned int load_count = 0;

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {

        uint64_t offset =
            ehdr.e_phoff +
            (uint64_t)i * ehdr.e_phentsize;

        if (offset > LONG_MAX ||
            fseek(file, (long)offset, SEEK_SET) != 0) {

            fprintf(stderr, "Error seeking program header\n");
            fclose(file);
            return -1;
        }

        Elf64_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, file) != 1) {
            fprintf(stderr, "Error reading program header\n");
            fclose(file);
            return -1;
        }

        if (phdr.p_type != PT_LOAD)
            continue;

        load_count++;

        printf("\nLOAD Segment %u\n", i);
        printf("  File offset : 0x%lx\n",
               (unsigned long)phdr.p_offset);
        printf("  Virtual addr: 0x%lx\n",
               (unsigned long)phdr.p_vaddr);
        printf("  File size   : 0x%lx\n",
               (unsigned long)phdr.p_filesz);
        printf("  Memory size : 0x%lx\n",
               (unsigned long)phdr.p_memsz);
        printf("  Alignment   : 0x%lx\n",
               (unsigned long)phdr.p_align);

        printf("  Permissions : %c%c%c\n",
               (phdr.p_flags & PF_R) ? 'R' : '-',
               (phdr.p_flags & PF_W) ? 'W' : '-',
               (phdr.p_flags & PF_X) ? 'X' : '-');

        if (phdr.p_memsz > phdr.p_filesz) {
            printf("  Zero-fill   : 0x%lx bytes\n",
                   (unsigned long)
                   (phdr.p_memsz - phdr.p_filesz));
        }
    }

    printf("\nTotal LOAD segments: %u\n", load_count);

    fclose(file);
    return 0;
}
