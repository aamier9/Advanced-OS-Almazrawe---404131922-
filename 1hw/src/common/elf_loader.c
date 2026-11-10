
#include "elf_loader.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <sys/mman.h>
#include <unistd.h>

/*
 * Release an ELF image and reset its fields.
 */
void elf_unload_image(elf_image_t *image)
{
    if (image == NULL)
        return;

    if (image->memory != NULL && image->size > 0)
        munmap(image->memory, image->size);

    image->memory = NULL;
    image->size = 0;
    image->min_vaddr = 0;
    image->entry_point = 0;
}

/*
 * Apply memory protection according to ELF PT_LOAD flags.
 *
 * Memory protection is applied at page granularity.
 *
 * Pages not belonging to a LOAD segment remain PROT_NONE.
 *
 * When multiple LOAD segments share a memory page,
 * their permissions are combined using a bitwise OR.
 * Therefore, the resulting page permissions represent
 * the union of permissions requested by those segments.
 *
 * This behavior reflects the page-level granularity
 * of Linux memory protection and may grant a permission
 * to bytes belonging to another segment in the same page.
 *
 * Writable + executable pages are rejected.
 */
static int elf_apply_protections(
    FILE *file,
    const Elf64_Ehdr *ehdr,
    void *memory,
    uint64_t aligned_start,
    size_t image_size,
    uint64_t page_size)
{
    if (file == NULL || ehdr == NULL || memory == NULL ||
        page_size == 0 || image_size == 0 ||
        image_size % page_size != 0) {
        fprintf(stderr, "Error: invalid protection arguments\n");
        return -1;
    }

    size_t page_count = image_size / (size_t)page_size;

    unsigned char *permissions = calloc(page_count, 1);

    if (permissions == NULL) {
        perror("calloc permissions");
        return -1;
    }

    /*
     * First pass: build a permission map for every page.
     */
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {

        uint64_t offset =
            ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize;

        if (offset > LONG_MAX ||
            fseek(file, (long)offset, SEEK_SET) != 0) {
            perror("fseek program header");
            free(permissions);
            return -1;
        }

        Elf64_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, file) != 1) {
            fprintf(stderr, "Error reading program header\n");
            free(permissions);
            return -1;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0)
            continue;

        /*
         * Calculate the page-aligned segment range.
         */
        uint64_t segment_start =
            phdr.p_vaddr - (phdr.p_vaddr % page_size);

        uint64_t segment_end =
            phdr.p_vaddr + phdr.p_memsz;

        if (segment_end > UINT64_MAX - (page_size - 1)) {
            fprintf(stderr, "Error: segment alignment overflow\n");
            free(permissions);
            return -1;
        }

        uint64_t aligned_end =
            ((segment_end + page_size - 1) / page_size)
            * page_size;

        /*
         * Ensure the segment belongs to the allocated image.
         */
        if (segment_start < aligned_start ||
            aligned_end < segment_start ||
            aligned_end - aligned_start > image_size) {
            fprintf(stderr,
                    "Error: segment outside allocated image\n");
            free(permissions);
            return -1;
        }

        size_t first_page =
            (size_t)((segment_start - aligned_start) / page_size);

        size_t last_page =
            (size_t)((aligned_end - aligned_start) / page_size);

        if (first_page >= page_count ||
            last_page > page_count ||
            first_page >= last_page) {
            fprintf(stderr, "Error: invalid page range\n");
            free(permissions);
            return -1;
        }

        /*
         * Convert ELF flags to mmap protection flags.
         */
        int protection = PROT_NONE;

        if (phdr.p_flags & PF_R)
            protection |= PROT_READ;

        if (phdr.p_flags & PF_W)
            protection |= PROT_WRITE;

        if (phdr.p_flags & PF_X)
            protection |= PROT_EXEC;

        /*
         * Assign permissions to every page in this segment.
         */
        for (size_t p = first_page; p < last_page; p++) {

            permissions[p] |= (unsigned char)protection;

            /*
             * Never allow a page to be writable and executable.
             */
            if ((permissions[p] & (PROT_WRITE | PROT_EXEC)) ==
                (PROT_WRITE | PROT_EXEC)) {

                fprintf(stderr,
                        "Error: page would be writable and executable\n");

                free(permissions);
                return -1;
            }
        }
    }

    /*
     * Second pass: apply protections using mprotect().
     *
     * Consecutive pages with identical permissions are grouped
     * into a single mprotect() call.
     */
    size_t start = 0;

    while (start < page_count) {

        unsigned char protection = permissions[start];

        size_t end = start + 1;

        while (end < page_count &&
               permissions[end] == protection) {
            end++;
        }

        void *address =
            (unsigned char *)memory +
            start * (size_t)page_size;

        size_t length =
            (end - start) * (size_t)page_size;

        if (mprotect(address, length, protection) != 0) {

            perror("mprotect");

            free(permissions);
            return -1;
        }

        start = end;
    }

    free(permissions);

    printf("ELF memory protections applied successfully\n");

    return 0;
}

/*
 * Load PT_LOAD segments into a separate memory image.
 *
 * This function:
 * 1. Validates the ELF file.
 * 2. Calculates the memory image range.
 * 3. Allocates writable memory.
 * 4. Copies LOAD segment data.
 * 5. Zero-fills BSS.
 * 6. Applies memory protections.
 *
 * It does NOT execute the loaded program.
 */
int elf_load_image(const char *filename, elf_image_t *image)
{
    if (filename == NULL || image == NULL) {
        fprintf(stderr, "Error: invalid loader arguments\n");
        return -1;
    }

    /*
     * Initialize output structure.
     */
    image->memory = NULL;
    image->size = 0;
    image->min_vaddr = 0;
    image->entry_point = 0;

    /*
     * Validate ELF header and LOAD segments.
     */
    if (elf_validate_segments(filename) != 0)
        return -1;

    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        perror("Error opening ELF file");
        return -1;
    }

    Elf64_Ehdr ehdr;

    if (fread(&ehdr, sizeof(ehdr), 1, file) != 1) {
        fprintf(stderr, "Error reading ELF header");
        fclose(file);
        return -1;
    }

    /*
     * Determine system page size.
     */
    long system_page_size = sysconf(_SC_PAGESIZE);

    if (system_page_size <= 0) {
        fprintf(stderr, "Error determining page size\n");
        fclose(file);
        return -1;
    }

    uint64_t page_size = (uint64_t)system_page_size;

    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;

    /*
     * First pass:
     * Calculate the total virtual memory range.
     */
    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {

        uint64_t offset =
            ehdr.e_phoff + (uint64_t)i * ehdr.e_phentsize;

        if (offset > LONG_MAX ||
            fseek(file, (long)offset, SEEK_SET) != 0) {

            fprintf(stderr, "Error reading program header\n");
            fclose(file);
            return -1;
        }

        Elf64_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, file) != 1) {
            fprintf(stderr, "Error reading program header\n");
            fclose(file);
            return -1;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0)
            continue;

        /*
         * Validation has already checked address overflow.
         */
        uint64_t segment_end =
            phdr.p_vaddr + phdr.p_memsz;

        if (phdr.p_vaddr < min_vaddr)
            min_vaddr = phdr.p_vaddr;

        if (segment_end > max_vaddr)
            max_vaddr = segment_end;
    }

    if (min_vaddr == UINT64_MAX) {
        fprintf(stderr, "Error: no usable LOAD segments\n");
        fclose(file);
        return -1;
    }

    /*
     * Align the complete image to page boundaries.
     */
    uint64_t aligned_start =
        min_vaddr - (min_vaddr % page_size);

    if (max_vaddr > UINT64_MAX - (page_size - 1)) {
        fprintf(stderr, "Error: memory range overflow\n");
        fclose(file);
        return -1;
    }

    uint64_t aligned_end =
        ((max_vaddr + page_size - 1) / page_size)
        * page_size;

    uint64_t image_size = aligned_end - aligned_start;

    if (image_size == 0 ||
        image_size > SIZE_MAX ||
        image_size % page_size != 0) {

        fprintf(stderr, "Error: invalid image size\n");
        fclose(file);
        return -1;
    }

    /*
     * Allocate a temporary writable memory image.
     *
     * Final permissions will be applied after copying.
     */
    void *memory = mmap(
        NULL,
        (size_t)image_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory == MAP_FAILED) {
        perror("mmap");
        fclose(file);
        return -1;
    }

    /*
     * Second pass:
     * Copy LOAD segment data and initialize BSS.
     */
    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {

        uint64_t offset =
            ehdr.e_phoff + (uint64_t)i * ehdr.e_phentsize;

        if (offset > LONG_MAX ||
            fseek(file, (long)offset, SEEK_SET) != 0) {

            fprintf(stderr, "Error reading program header\n");
            munmap(memory, (size_t)image_size);
            fclose(file);
            return -1;
        }

        Elf64_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, file) != 1) {

            fprintf(stderr, "Error reading program header\n");
            munmap(memory, (size_t)image_size);
            fclose(file);
            return -1;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0)
            continue;

        uint64_t relative_offset =
            phdr.p_vaddr - aligned_start;

        if (relative_offset > image_size ||
            phdr.p_memsz > image_size - relative_offset ||
            relative_offset > SIZE_MAX ||
            phdr.p_filesz > SIZE_MAX ||
            phdr.p_memsz > SIZE_MAX) {

            fprintf(stderr,
                    "Error: segment outside allocated image\n");

            munmap(memory, (size_t)image_size);
            fclose(file);
            return -1;
        }

        unsigned char *destination =
            (unsigned char *)memory +
            (size_t)relative_offset;

        /*
         * Copy initialized bytes from the ELF file.
         */
        if (phdr.p_filesz > 0) {

            if (phdr.p_offset > LONG_MAX ||
                fseek(file, (long)phdr.p_offset, SEEK_SET) != 0) {

                fprintf(stderr, "Error seeking to segment data\n");
                munmap(memory, (size_t)image_size);
                fclose(file);
                return -1;
            }

            if (fread(
                    destination,
                    1,
                    (size_t)phdr.p_filesz,
                    file
                ) != (size_t)phdr.p_filesz) {

                fprintf(stderr, "Error reading segment data\n");
                munmap(memory, (size_t)image_size);
                fclose(file);
                return -1;
            }
        }

        /*
         * Zero-fill the remaining memory (BSS).
         */
        if (phdr.p_memsz > phdr.p_filesz) {

            memset(
                destination + (size_t)phdr.p_filesz,
                0,
                (size_t)(phdr.p_memsz - phdr.p_filesz)
            );
        }
    }

    /*
     * Apply final page protections after all writes are complete.
     */
    if (elf_apply_protections(
            file,
            &ehdr,
            memory,
            aligned_start,
            (size_t)image_size,
            page_size
        ) != 0) {

        munmap(memory, (size_t)image_size);
        fclose(file);
        return -1;
    }

    fclose(file);

    /*
     * Publish the successfully loaded image.
     */
    image->memory = memory;
    image->size = (size_t)image_size;
    image->min_vaddr = aligned_start;
    image->entry_point = ehdr.e_entry;

    printf("ELF image loaded successfully\n");
    printf("Memory base: %p\n", image->memory);
    printf("Virtual base: 0x%lx\n",
           (unsigned long)image->min_vaddr);
    printf("Image size: %zu bytes\n", image->size);
    printf("Entry point: 0x%lx\n",
           (unsigned long)image->entry_point);

    return 0;
}
