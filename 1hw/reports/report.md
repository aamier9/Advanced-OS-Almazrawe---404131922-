# Advanced Operating Systems – Lab 1 Report
## Program Loading and Memory Management

**Student Name:** Amer Sami Waheed Al-Mazrawe
**Student ID:** 404131922
**Course:** Advanced Operating Systems
**Programming Language:** C
**Compiler:** GCC

---

## 1. Introduction and Objectives

The objective of this laboratory project is to study and implement fundamental operating-system concepts related to program loading, virtual memory, paging, memory protection, and memory management.

The project includes the following implementation areas:

1. Pager simulations: APAGER, HPAGER, and DPAGER.
2. Demand paging using virtual memory and page-fault handling.
3. Hybrid paging with initial page loading and prefetching.
4. ELF64 executable validation and memory-image loading.
5. LOAD segment processing and BSS initialization.
6. Page-level memory protection.
7. Memory allocation and release.
8. User-stack management.
9. Dynamic linking and malloc-hook testing.
10. Memory-access and performance experiments.

The implementation is an educational user-space project. It does not constitute a complete operating-system kernel or a production ELF runtime loader.

---

## 2. Project Architecture

The project is organized under the `1hw/` directory.

### 2.1 Important Source Files

| Component | Source Files |
|---|---|
| ELF Parser | `src/common/elf_parser.c` |
| ELF Loader | `src/common/elf_loader.c` |
| Memory Management | `src/common/memory.c` |
| Stack Management | `src/common/stack.c` |
| Original Heuristic Pager | `src/hpager/hpager.c` |
| Original Dynamic Pager | `src/dpager/dpager.c` |
| Demand Pager | `src/dpager/demand_pager.c` |
| Hybrid Pager | `src/hpager/hybrid_pager.c` |
| Memory Interface | `include/memory.h` |
| Stack Interface | `include/stack.h` |
| Demand Pager Interface | `include/demand_pager.h` |
| Hybrid Pager Interface | `include/hybrid_pager.h` |
| ELF Loader Interface | `include/elf_loader.h` |
| Build Configuration | `Makefile` |
| Test Programs | `tests/` |
| Project Report | `reports/report.md` |

The project contains both the original pager simulations and additional implementations based on Linux virtual-memory mechanisms.

---

## 3. Original Pager Simulations

The original pager tests cover three pager implementations.

### 3.1 APAGER

APAGER handles a page fault by assigning a frame to the requested virtual page.

The implemented frame mapping is:

\[
Frame\ Number = VPN + 100
\]

The implementation updates the page-table entry and marks the page as present.

### 3.2 HPAGER

HPAGER implements a heuristic prefetching strategy.

When a page fault occurs, it loads the requested page and anticipates nearby page accesses within a configured prefetch window.

The prefetch limit is two pages.

### 3.3 DPAGER

DPAGER tracks page-access information.

The implemented frame mapping is:

\[
Frame\ Number = VPN + 300
\]

The implementation also updates the page hit counter.

### 3.4 Original Pager Test Results

```text
APAGER: PASS
HPAGER: PASS
DPAGER: PASS
```

---

## 4. Demand Paging

### 4.1 Implementation Overview

The Demand Pager is implemented in:

- `src/dpager/demand_pager.c`
- `include/demand_pager.h`

The implementation reserves a virtual-memory region using Linux `mmap()` with `PROT_NONE`.

Initially, the managed pages are inaccessible. When a process accesses an unloaded page, a `SIGSEGV` signal is generated.

The registered signal handler identifies whether the fault address belongs to the managed memory region.

If the address is valid, the handler changes the page permissions using `mprotect()` and marks the page as loaded.

### 4.2 Main Operations

1. Obtain the system page size.
2. Calculate the required page count.
3. Reserve virtual memory using `mmap()`.
4. Allocate page-tracking information.
5. Register a `SIGSEGV` handler.
6. Enable access to a page when it is first accessed.
7. Track loaded pages and page-fault counts.
8. Release the allocated memory and restore the previous signal handler.

### 4.3 Demand Pager Test

The Demand Pager test verifies:

- Successful initialization.
- Initial reservation of inaccessible memory.
- Page activation after an access fault.
- Page-fault counting.
- Loaded-page tracking.
- Resource cleanup.

**Result: PASS**

The implementation is a user-space demonstration of demand paging. It does not implement kernel-level page-table management.

---

## 5. Hybrid Paging

### 5.1 Implementation Overview

The Hybrid Pager is implemented in:

- `src/hpager/hybrid_pager.c`
- `include/hybrid_pager.h`

It combines initial loading of pages containing initialized ELF file data with demand loading and prefetching.

The ELF image is inspected to identify its LOAD segments and the memory pages associated with them.

The implementation creates a memory region using `mmap()` with `PROT_NONE`, prepares page metadata, and stages the ELF segment contents.

### 5.2 Loading Strategy

The Hybrid Pager follows these steps:

1. Validate the ELF file and its LOAD segments.
2. Calculate the virtual-memory image range.
3. Allocate page metadata and staging memory.
4. Reserve the image using `mmap()`.
5. Identify valid pages belonging to LOAD segments.
6. Copy initialized file data into the staging image.
7. Preload pages containing initialized file data.
8. Leave pages without initialized file data unloaded.
9. Handle page faults when an unloaded valid page is accessed.
10. Prefetch the next two valid, unloaded pages.

The BSS region is zero-initialized through the zero-filled staging memory.

### 5.3 Hybrid Pager Test Results

The latest test run reported:

| Metric | Result |
|---|---:|
| Page size | 4,096 bytes |
| Total pages | 184 |
| Preloaded pages | 178 |
| Demand pages | 1 |
| Prefetched pages | 2 |
| Page faults | 1 |
| Loaded pages | 181 |

The test verified initial loading, demand loading of an unloaded BSS page, zero initialization, prefetching, counters, and cleanup.

**Result: PASS**

The Hybrid Pager is an educational user-space simulation. It does not execute the loaded ELF image or implement a kernel-level executable loader.

---

## 6. ELF Header Validation

The ELF parser validates the following properties:

- ELF magic number.
- ELF64 class.
- Little-endian byte order.
- ELF version.
- x86-64 architecture.
- Executable file type (`ET_EXEC`).
- ELF header and program-header entry sizes.
- Program-header table boundaries.

The current implementation supports ELF64 x86-64 `ET_EXEC` files and rejects PIE executables (`ET_DYN`).

### 6.1 Validation Test Results

| Test | Result |
|---|---|
| Static ELF executable | Accepted |
| PIE executable | Rejected as expected |
| Program-header table | Validated successfully |

### 6.2 Negative Validation Tests

Two malformed ELF test cases were used:

- **Invalid entry point:** The entry point was changed to `0x400100`, which belongs to a non-executable LOAD segment. The parser rejected the file.
- **Overlapping LOAD segments:** The first LOAD segment was modified to overlap with the second LOAD segment. The parser detected and rejected the overlap.

Both negative validation tests passed.

---

## 7. LOAD Segment Validation

The parser examines `PT_LOAD` program headers.

The implemented checks include:

1. File size must not exceed memory size.
2. Segment data must remain within file boundaries.
3. Virtual-address calculations must not overflow.
4. Segment alignment must be valid.
5. Unsupported permission flags are rejected.
6. Writable and executable segments are rejected.
7. Overlapping LOAD memory ranges are rejected.

The test ELF executable contained four LOAD segments.

| Segment | Permissions |
|---|---|
| LOAD 1 | Read |
| LOAD 2 | Read + Execute |
| LOAD 3 | Read |
| LOAD 4 | Read + Write |

All four LOAD segments passed validation.

---

## 8. ELF Memory Loading

The ELF loader creates a separate memory image using Linux `mmap()`.

The implementation performs the following operations:

1. Validates the ELF file and its LOAD segments.
2. Calculates the required virtual-memory range.
3. Aligns image boundaries to system page boundaries.
4. Allocates anonymous writable memory.
5. Copies initialized segment data from the ELF file.
6. Initializes the remaining BSS region to zero.
7. Applies final memory permissions.
8. Releases resources when loading fails or when the image is unloaded.

The implementation creates a memory copy of the ELF layout. It does not execute the loaded program or map it at its original virtual addresses.

### 8.1 Memory Loading Test Results

The test executable was a statically linked ELF64 x86-64 executable.

| Test | Result |
|---|---|
| Four LOAD segments processed | PASS |
| Segment data copied correctly | PASS |
| BSS zero initialization | PASS |
| Entry point checked against allocated image | PASS |
| Memory released successfully | PASS |

Additional measurements:

- BSS zero-filled region: 22,560 bytes.
- Allocated image size: 753,664 bytes.

---

## 9. Memory Protection

Memory protection is implemented using Linux `mprotect()`.

The loader translates ELF segment flags into Linux memory-protection flags.

| ELF Flag | Linux Protection |
|---|---|
| `PF_R` | `PROT_READ` |
| `PF_W` | `PROT_WRITE` |
| `PF_X` | `PROT_EXEC` |

### 9.1 Page-Level Permission Handling

Memory protection is applied at page granularity rather than individual bytes.

When multiple LOAD segments share the same memory page, their permissions are combined using a bitwise OR operation.

The resulting page receives the union of the permissions requested by all segments occupying that page.

The implementation rejects any resulting page that would be both writable and executable (W+X).

Pages that do not belong to a LOAD segment remain `PROT_NONE`.

### 9.2 Write Protection Test

A child process attempted to write to a read-only memory page.

The operating system generated `SIGSEGV`, confirming that the write was blocked.

```text
PASS: Write to read-only page blocked (SIGSEGV)
```

### 9.3 Execute Protection Test

A child process attempted to execute instructions from a non-executable memory page.

The operating system generated `SIGSEGV`, confirming that execution was blocked.

```text
PASS: Execution from non-executable page blocked (SIGSEGV)
```

### 9.4 Memory Release Test

The allocated image was successfully released using `munmap()`.

```text
PASS: Memory released successfully
```

---

## 10. Memory Management and Stack

The project includes memory-management and stack components.

### 10.1 Memory Management

The memory-management implementation is located in:

- `src/common/memory.c`
- `include/memory.h`

The memory tests verify the implemented allocation and release operations.

**Result: PASS**

### 10.2 Stack Management

The stack implementation is located in:

- `src/common/stack.c`
- `include/stack.h`

The stack test program is `tests/stack_test.c`.

The implementation was tested for the following operations:

- Correct handling of `argc` and `argv`.
- Environment variable pointer handling through `envp`.
- Auxiliary vector setup.
- Stack alignment.
- Stack memory release.

All the above test cases passed in the latest full test execution.

**Result: PASS**

These tests verify the implemented stack-construction operations. They do not establish a complete Linux process startup environment.

---

## 11. Dynamic Linking

The project includes a dynamic-linking demonstration based on the Linux `LD_PRELOAD` mechanism.

Important files:

- `dynamic_linking/malloc_hook.c`
- `dynamic_linking/test_malloc.c`

The shared library intercepts memory-allocation requests made through the implemented malloc hook.

The test was executed using:

```bash
LD_PRELOAD="/home/alaraby/Advanced-OS/1hw/dynamic_linking/libmalloc_hook.so" ./dynamic_linking/test_malloc
```

The test verified:

- Successful loading of the malloc-hook shared library.
- Interception of allocation requests for 64 bytes and 1,024 bytes.
- Verification that the first 64 allocated bytes contain the expected `0xCC` pattern.

**Result: PASS**

This is a demonstration of function interception using `LD_PRELOAD`. It does not implement complete ELF dynamic linking, relocation processing, or PIE executable support.

## 12. Performance and Memory Experiments

Three memory experiments were executed using a 64 MB memory region and a 4,096-byte system page size.

### 12.1 Performance Results

| Metric | Array Test | BSS Test | Sequential Test |
|---|---:|---:|---:|
| Memory size | 64 MB | 64 MB | 64 MB |
| Page size | 4,096 B | 4,096 B | 4,096 B |
| Touched pages | 16,384 | 16,384 | 16,384 |
| Execution time | 0.036568 s | 0.047054 s | 0.043867 s |
| Minor faults | 16,386 | 32,768 | 16,386 |
| Major faults | 0 | 0 | 0 |
| Checksum | 4,177,920 | 16,384 | 2,088,960 |
| Memory released | PASS | PASS | PASS |

### 12.2 Interpretation

All three experiments completed successfully and verified their expected checksums and memory-release behavior.

The reported execution times represent individual test runs and should not be interpreted as statistically conclusive performance comparisons.

The minor and major fault counts are obtained from process-level resource usage measurements. They represent the process as a whole and are not necessarily attributable exclusively to the tested array or memory region.

In particular, minor page faults should not be interpreted as exactly one fault per touched page.

These experiments demonstrate memory-access behavior under the test environment. They are not a direct benchmark comparing the internal performance of different pager implementations.

---

## 13. Complete Test Summary

The latest successful `make check` execution completed all fourteen test groups.

| No. | Test Group | Result |
|---:|---|---|
| 1 | APAGER / HPAGER / DPAGER | PASS |
| 2 | ELF Layout | PASS |
| 3 | ELF Validation | PASS |
| 4 | ELF Loading | PASS |
| 5 | ELF Protection | PASS |
| 6 | Memory Management | PASS |
| 7 | User Stack | PASS |
| 8 | Dynamic Linking | PASS |
| 9 | Demand Paging | PASS |
| 10 | Invalid Memory Access / SIGSEGV | PASS |
| 11 | Hybrid Paging | PASS |
| 12 | Array Memory | PASS |
| 13 | BSS Memory | PASS |
| 14 | Sequential Access | PASS |

Final test-runner output:

```text
========================================
       ALL TESTS COMPLETED
========================================
```

**Overall result: All fourteen test groups passed in the latest recorded `make check` execution.**

---

## 14. Development Environment

The implementation and tests were developed and executed in a Linux environment under WSL.

| Item | Environment |
|---|---|
| Operating System | Ubuntu 26.04.1 LTS |
| Codename | Resolute |
| Compiler | GCC |
| Programming Language | C |
| Build Standard | C99 |
| Compiler Options | `-Wall -Wextra -Wpedantic` |
| Build System | Make |

The assignment's specified Ubuntu 24.04 environment still requires separate compatibility verification.

---

## 15. Limitations and Future Work

The current project provides educational implementations of memory-management and ELF-loading concepts. Some capabilities of a complete operating-system executable loader remain outside the current implementation.

The following capabilities are not implemented as a complete runtime-loading system:

- Loading an executable at its original virtual addresses.
- Transferring control to the loaded ELF entry point.
- Complete process execution setup.
- Full initial process stack and execution-state setup.
- Supporting PIE (`ET_DYN`) executables.
- Full dynamic linking and relocation processing.
- Kernel-level page-table management.

The Demand Pager and Hybrid Pager use user-space signal handling and memory-protection mechanisms to demonstrate paging behavior. They are not replacements for kernel page-fault handling.

The signal-handler implementations are educational prototypes and are not intended as production-grade asynchronous-signal-safe memory-management mechanisms.

Further work should follow the remaining laboratory requirements, particularly executable loading and execution setup, and any additional components specified by the assignment.

---

## 16. Conclusion

This laboratory project implements several operating-system memory-management concepts in C.

The original pager simulations, Demand Pager, and Hybrid Pager demonstrate different approaches to page tracking, demand loading, and prefetching.

The ELF implementation validates ELF64 headers and LOAD segments, copies segment data into a separate memory image, initializes BSS, and applies page-level memory protections.

Additional components cover memory management, stack operations, dynamic-linking tests, invalid memory access, and memory-access experiments.

The latest recorded full test execution passed all fourteen test groups.

Overall, the project provides a practical educational foundation for understanding virtual memory, paging, ELF structures, memory protection, and program-loading concepts in Linux.