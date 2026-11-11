
#define _GNU_SOURCE

#include "demand_pager.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static demand_pager_t *active_pager = NULL;
static struct sigaction previous_action;
static volatile sig_atomic_t handler_installed = 0;

/*
 * Handle a page fault inside the managed memory region.
 */
static void demand_segv_handler(int signal_number,
                                siginfo_t *info,
                                void *context)
{
    (void)context;

    if (signal_number == SIGSEGV &&
        info != NULL &&
        active_pager != NULL &&
        info->si_code == SEGV_ACCERR) {

        uintptr_t fault_address = (uintptr_t)info->si_addr;
        uintptr_t start = (uintptr_t)active_pager->address;
        uintptr_t end = start + active_pager->size;

        if (fault_address >= start && fault_address < end) {

            size_t page_index =
                (fault_address - start) / active_pager->page_size;

            if (page_index < active_pager->page_count &&
                !active_pager->loaded_pages[page_index]) {

                void *page_address =
                    (void *)(start +
                    page_index * active_pager->page_size);

                if (mprotect(page_address,
                             active_pager->page_size,
                             PROT_READ | PROT_WRITE) == 0) {

                    active_pager->loaded_pages[page_index] = 1;

                    if (active_pager->page_faults < SIG_ATOMIC_MAX) {
                        active_pager->page_faults++;
                    }

                    return;
                }

                _exit(128 + SIGSEGV);
            }
        }
    }

    /*
     * The fault does not belong to our pager.
     * Restore the previous handler and re-deliver SIGSEGV.
     */
    if (handler_installed) {
        struct sigaction old_action = previous_action;

        handler_installed = 0;
        active_pager = NULL;

        /*
         * Ignoring a synchronous invalid memory access cannot
         * safely resume the faulting instruction.
         */
        if (old_action.sa_handler == SIG_IGN) {
            old_action.sa_handler = SIG_DFL;
        }

        if (sigaction(SIGSEGV, &old_action, NULL) != 0) {
            _exit(128 + SIGSEGV);
        }

        if (kill(getpid(), SIGSEGV) != 0) {
            _exit(128 + SIGSEGV);
        }

        return;
    }

    _exit(128 + SIGSEGV);
}

/*
 * Reserve virtual memory without loading its pages.
 */
int demand_pager_init(demand_pager_t *pager, size_t size)
{
    if (pager == NULL || size == 0 || active_pager != NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(pager, 0, sizeof(*pager));

    long system_page_size = sysconf(_SC_PAGESIZE);

    if (system_page_size <= 0) {
        return -1;
    }

    pager->page_size = (size_t)system_page_size;

    if (size > SIZE_MAX - (pager->page_size - 1)) {
        errno = EOVERFLOW;
        return -1;
    }

    pager->page_count =
        (size + pager->page_size - 1) / pager->page_size;

    if (pager->page_count > SIZE_MAX / sizeof(sig_atomic_t)) {
        errno = EOVERFLOW;
        return -1;
    }

    pager->size = pager->page_count * pager->page_size;

    pager->address = mmap(NULL,
                          pager->size,
                          PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0);

    if (pager->address == MAP_FAILED) {
        pager->address = NULL;
        return -1;
    }

    size_t tracking_size =
        pager->page_count * sizeof(sig_atomic_t);

    pager->loaded_pages =
        mmap(NULL,
             tracking_size,
             PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS,
             -1,
             0);

    if (pager->loaded_pages == MAP_FAILED) {
        pager->loaded_pages = NULL;
        munmap(pager->address, pager->size);
        pager->address = NULL;
        return -1;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));

    action.sa_sigaction = demand_segv_handler;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGSEGV, &action, &previous_action) != 0) {
        munmap((void *)pager->loaded_pages, tracking_size);
        munmap(pager->address, pager->size);

        pager->loaded_pages = NULL;
        pager->address = NULL;
        return -1;
    }

    active_pager = pager;
    handler_installed = 1;
    pager->active = 1;

    return 0;
}

/*
 * Release the managed memory and restore the previous signal action.
 */
void demand_pager_destroy(demand_pager_t *pager)
{
    if (pager == NULL || !pager->active) {
        return;
    }

    if (active_pager == pager) {
        active_pager = NULL;

        if (handler_installed) {
            sigaction(SIGSEGV, &previous_action, NULL);
            handler_installed = 0;
        }
    }

    if (pager->address != NULL) {
        munmap(pager->address, pager->size);
    }

    if (pager->loaded_pages != NULL) {
        size_t tracking_size =
            pager->page_count * sizeof(sig_atomic_t);

        munmap((void *)pager->loaded_pages, tracking_size);
    }

    memset(pager, 0, sizeof(*pager));
}

/*
 * Return the number of pages loaded so far.
 */
size_t demand_pager_loaded_count(const demand_pager_t *pager)
{
    if (pager == NULL || pager->loaded_pages == NULL) {
        return 0;
    }

    size_t count = 0;

    for (size_t i = 0; i < pager->page_count; i++) {
        if (pager->loaded_pages[i]) {
            count++;
        }
    }

    return count;
}

