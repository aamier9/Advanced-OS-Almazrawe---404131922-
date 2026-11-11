
#define _GNU_SOURCE

#include "hybrid_pager.h"
#include "elf_loader.h"

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/types.h>

typedef struct {
    int valid;
    int protection;
    int has_file_data;
} hybrid_page_info_t;

typedef struct {
    hybrid_page_info_t *pages;
    unsigned char *staging;
    struct sigaction previous_action;
    int handler_installed;
} hybrid_private_t;

static hybrid_pager_t *active_pager = NULL;

static int elf_flags_to_protection(uint32_t flags)
{
    int protection = PROT_NONE;

    if (flags & PF_R)
        protection |= PROT_READ;

    if (flags & PF_W)
        protection |= PROT_WRITE;

    if (flags & PF_X)
        protection |= PROT_EXEC;

    return protection;
}

static int activate_page(
    hybrid_pager_t *pager,
    size_t index,
    sig_atomic_t state)
{
    hybrid_private_t *private_data =
        (hybrid_private_t *)pager->private_data;

    if (index >= pager->page_count ||
        !private_data->pages[index].valid)
        return -1;

    unsigned char *destination =
        (unsigned char *)pager->address +
        index * pager->page_size;

    const unsigned char *source =
        private_data->staging +
        index * pager->page_size;

    if (mprotect(
            destination,
            pager->page_size,
            PROT_READ | PROT_WRITE) != 0)
        return -1;

    memcpy(destination, source, pager->page_size);

    if (mprotect(
            destination,
            pager->page_size,
            private_data->pages[index].protection) != 0)
        return -1;

    pager->page_states[index] = state;
    return 0;
}

static void hybrid_segv_handler(
    int signal_number,
    siginfo_t *info,
    void *context)
{
    hybrid_pager_t *pager = active_pager;

    if (pager == NULL || !pager->active || info == NULL)
        goto forward_signal;

    uintptr_t fault_address = (uintptr_t)info->si_addr;
    uintptr_t base = (uintptr_t)pager->address;

    if (fault_address < base ||
        fault_address - base >= pager->size)
        goto forward_signal;

    size_t offset = (size_t)(fault_address - base);
    size_t index = offset / pager->page_size;

    if (index >= pager->page_count ||
        pager->page_states[index] != 0)
        goto forward_signal;

    hybrid_private_t *private_data =
        (hybrid_private_t *)pager->private_data;

    if (!private_data->pages[index].valid)
        goto forward_signal;

    if (activate_page(pager, index, 2) != 0)
        goto forward_signal;

    pager->page_faults++;
    pager->demand_pages++;

    /*
     * Prefetch the next two valid, unloaded pages.
     */
    size_t prefetched = 0;

    for (size_t next = index + 1;
         next < pager->page_count && prefetched < 2;
         next++) {

        if (!private_data->pages[next].valid ||
            pager->page_states[next] != 0)
            continue;

        if (activate_page(pager, next, 3) == 0) {
            pager->prefetched_pages++;
            prefetched++;
        }
    }

    return;

forward_signal:
    /*
     * Restore the previous SIGSEGV action and re-raise.
     * This is a simple educational forwarding strategy.
     */
    if (pager != NULL && pager->private_data != NULL) {
        hybrid_private_t *data =
            (hybrid_private_t *)pager->private_data;

        if (data->handler_installed)
            sigaction(SIGSEGV, &data->previous_action, NULL);
    }

    signal(signal_number, SIG_DFL);
    raise(signal_number);
    (void)context;
}

size_t hybrid_pager_loaded_count(const hybrid_pager_t *pager)
{
    if (pager == NULL || pager->page_states == NULL)
        return 0;

    size_t count = 0;

    for (size_t i = 0; i < pager->page_count; i++) {
        if (pager->page_states[i] != 0)
            count++;
    }

    return count;
}

void hybrid_pager_destroy(hybrid_pager_t *pager)
{
    if (pager == NULL)
        return;

    hybrid_private_t *private_data =
        (hybrid_private_t *)pager->private_data;

    if (private_data != NULL) {
        if (private_data->handler_installed) {
            sigaction(
                SIGSEGV,
                &private_data->previous_action,
                NULL);
        }

        free(private_data->pages);
        free(private_data->staging);
        free(private_data);
    }

    if (pager->address != NULL && pager->size != 0)
        munmap(pager->address, pager->size);

    free((void *)pager->page_states);

    if (active_pager == pager)
        active_pager = NULL;

    memset(pager, 0, sizeof(*pager));
}

int hybrid_pager_load(
    const char *filename,
    hybrid_pager_t *pager)
{
    if (filename == NULL || pager == NULL) {
        fprintf(stderr, "Invalid Hybrid Pager arguments\n");
        return -1;
    }

    memset(pager, 0, sizeof(*pager));

    if (active_pager != NULL) {
        fprintf(stderr,
                "Only one active Hybrid Pager is supported\n");
        return -1;
    }

    if (elf_validate_segments(filename) != 0)
        return -1;

    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    Elf64_Ehdr ehdr;

    if (fread(&ehdr, sizeof(ehdr), 1, file) != 1) {
        fclose(file);
        return -1;
    }

    long system_page_size = sysconf(_SC_PAGESIZE);

    if (system_page_size <= 0) {
        fclose(file);
        return -1;
    }

    size_t page_size = (size_t)system_page_size;

    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        uint64_t offset =
            ehdr.e_phoff +
            (uint64_t)i * ehdr.e_phentsize;

        if (offset > LONG_MAX ||
            fseek(file, (long)offset, SEEK_SET) != 0) {
            fclose(file);
            return -1;
        }

        Elf64_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, file) != 1) {
            fclose(file);
            return -1;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0)
            continue;

        uint64_t end = phdr.p_vaddr + phdr.p_memsz;

        if (phdr.p_vaddr < min_vaddr)
            min_vaddr = phdr.p_vaddr;

        if (end > max_vaddr)
            max_vaddr = end;
    }

    if (min_vaddr == UINT64_MAX ||
        max_vaddr > UINT64_MAX - (page_size - 1)) {
        fclose(file);
        return -1;
    }

    uint64_t aligned_start =
        min_vaddr - (min_vaddr % page_size);

    uint64_t aligned_end =
        ((max_vaddr + page_size - 1) / page_size) *
        page_size;

    uint64_t image_size = aligned_end - aligned_start;

    if (image_size == 0 ||
        image_size > SIZE_MAX ||
        image_size % page_size != 0) {
        fclose(file);
        return -1;
    }

    size_t total_size = (size_t)image_size;
    size_t page_count = total_size / page_size;

    hybrid_private_t *private_data =
        calloc(1, sizeof(*private_data));

    if (private_data == NULL) {
        fclose(file);
        return -1;
    }

    private_data->pages =
        calloc(page_count, sizeof(hybrid_page_info_t));

    private_data->staging =
        calloc(1, total_size);

    pager->page_states =
        calloc(page_count, sizeof(sig_atomic_t));

    if (private_data->pages == NULL ||
        private_data->staging == NULL ||
        pager->page_states == NULL) {
        free(private_data->pages);
        free(private_data->staging);
        free(private_data);
        free((void *)pager->page_states);
        pager->page_states = NULL;
        fclose(file);
        return -1;
    }

    void *address = mmap(
        NULL,
        total_size,
        PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);

    if (address == MAP_FAILED) {
        free(private_data->pages);
        free(private_data->staging);
        free(private_data);
        free((void *)pager->page_states);
        pager->page_states = NULL;
        fclose(file);
        return -1;
    }

    pager->address = address;
    pager->size = total_size;
    pager->page_size = page_size;
    pager->page_count = page_count;
    pager->min_vaddr = aligned_start;
    pager->entry_point = ehdr.e_entry;
    pager->private_data = private_data;

    /*
     * Build page metadata and copy segment file data
     * into the staging image.
     */
    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        uint64_t offset =
            ehdr.e_phoff +
            (uint64_t)i * ehdr.e_phentsize;

        if (offset > LONG_MAX ||
            fseek(file, (long)offset, SEEK_SET) != 0)
            goto fail;

        Elf64_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, file) != 1)
            goto fail;

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0)
            continue;

        uint64_t relative =
            phdr.p_vaddr - aligned_start;

        if (relative > total_size ||
            phdr.p_memsz > total_size - relative ||
            phdr.p_filesz > SIZE_MAX)
            goto fail;

        size_t first =
            (size_t)(relative / page_size);

        size_t last =
            (size_t)((relative + phdr.p_memsz +
                      page_size - 1) / page_size);

        int protection =
            elf_flags_to_protection(phdr.p_flags);

        for (size_t p = first; p < last; p++) {
            private_data->pages[p].valid = 1;
            private_data->pages[p].protection |= protection;

            if ((private_data->pages[p].protection &
                 (PROT_WRITE | PROT_EXEC)) ==
                (PROT_WRITE | PROT_EXEC))
                goto fail;
        }

        if (phdr.p_filesz > 0) {
            if (phdr.p_offset > LONG_MAX ||
                fseek(file, (long)phdr.p_offset, SEEK_SET) != 0)
                goto fail;

            if (fread(
                    private_data->staging + (size_t)relative,
                    1,
                    (size_t)phdr.p_filesz,
                    file) != (size_t)phdr.p_filesz)
                goto fail;

            size_t file_first = first;
            size_t file_last =
                (size_t)((relative + phdr.p_filesz +
                          page_size - 1) / page_size);

            for (size_t p = file_first; p < file_last; p++)
                private_data->pages[p].has_file_data = 1;
        }
    }

    fclose(file);
    file = NULL;

    /*
     * Preload pages containing initialized file bytes.
     */
    for (size_t p = 0; p < page_count; p++) {
        if (!private_data->pages[p].valid ||
            !private_data->pages[p].has_file_data)
            continue;

        if (activate_page(pager, p, 1) != 0)
            goto fail;

        pager->preloaded_pages++;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));

    action.sa_sigaction = hybrid_segv_handler;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);

    if (sigaction(
            SIGSEGV,
            &action,
            &private_data->previous_action) != 0)
        goto fail;

    private_data->handler_installed = 1;
    pager->active = 1;
    active_pager = pager;

    printf("Hybrid Pager initialized successfully\n");
    printf("Virtual base: 0x%lx\n",
           (unsigned long)pager->min_vaddr);
    printf("Image size: %zu bytes\n", pager->size);
    printf("Total pages: %zu\n", pager->page_count);
    printf("Preloaded pages: %zu\n", pager->preloaded_pages);

    return 0;

fail:
    if (file != NULL)
        fclose(file);

    hybrid_pager_destroy(pager);
    return -1;
}
