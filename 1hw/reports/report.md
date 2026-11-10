# Advanced Operating Systems – Lab 1 Report

**Student Name:** Amer Sami Waheed Al-Mazrawe
**Student ID:** 404131922
**Course:** Advanced Operating Systems

---

## 1. Introduction and Objectives

The objective of this laboratory project is to study and implement fundamental operating-system memory-management concepts in C.

The project includes two main implementation areas:

1. Pager simulations, including demand paging, heuristic prefetching, and dynamic page tracking.
2. ELF file validation and memory loading, including LOAD segment processing, BSS initialization, and memory protection.

The implementation is intended for educational purposes and does not constitute a complete operating-system kernel or a production ELF runtime loader.

---

## 2. Project Architecture

The project is organized under the `1hw/` directory.

Important source files:

- `src/apager/apager.c`: Demand paging implementation.
- `src/hpager/hpager.c`: Heuristic paging implementation.
- `src/dpager/dpager.c`: Dynamic paging implementation.
- `src/common/elf_parser.c`: ELF header and program-segment validation.
- `src/common/elf_loader.c`: ELF memory-image loading and memory protection.
- `include/pager.h`: Shared pager definitions.
- `include/elf_loader.h`: ELF loader structures and function declarations.
- `tests/`: Pager, ELF, and memory-mapping test programs.
- `reports/report.md`: Project documentation.
- `Makefile`: Build automation.

---

## 3. Pager Implementations

### 3.1 Demand Pager (APAGER)

The demand pager handles a page fault by assigning a frame to the requested virtual page.

The implemented frame mapping is:

\[
Frame\ Number = VPN + 100
\]

The implementation updates the page-table entry and marks the page as present.

### 3.2 Heuristic Pager (HPAGER)

The heuristic pager implements a prefetching strategy.

When a page fault occurs, it loads the requested page and anticipates nearby page accesses within the configured prefetch window.

The implementation uses a prefetch limit of two pages.

### 3.3 Dynamic Pager (DPAGER)

The dynamic pager tracks page-access information.

The implemented frame mapping is:

\[
Frame\ Number = VPN + 300
\]

The implementation also updates the page hit counter.

### 3.4 Pager Test Results

The pager test suite reported:

```text
=== Advanced OS Pager Tests ===
APAGER: PASS
HPAGER: PASS
DPAGER: PASS
=== All Pager Tests Passed ===
```

---

## 4. ELF Header Validation

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

### Validation Test Results

- Static ELF executable: Accepted.
- PIE executable: Rejected as expected.
- Program-header table: Validated successfully.

---
### Negative Validation Tests

Two malformed ELF test files were created by modifying copies of the original static ELF executable.

- Invalid entry point: The entry point was changed to 0x400100, which belongs to a non-executable LOAD segment. The parser correctly rejected the file.
- Overlapping LOAD segments: The first LOAD segment was modified to overlap with the second LOAD segment. The parser correctly detected and rejected the overlap.

Both negative tests passed.

## 5. LOAD Segment Validation

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

The segment permissions were:

| Segment | Permissions |
|---|---|
| LOAD 1 | Read |
| LOAD 2 | Read + Execute |
| LOAD 3 | Read |
| LOAD 4 | Read + Write |

All four LOAD segments passed validation.

---

## 6. ELF Memory Loading

The ELF loader creates a separate memory image using Linux `mmap()`.

The implementation performs these operations:

1. Validates the ELF file and its LOAD segments.
2. Calculates the required virtual memory range.
3. Aligns the image boundaries to system page boundaries.
4. Allocates anonymous writable memory.
5. Copies initialized segment data from the ELF file.
6. Initializes the remaining BSS region to zero.
7. Applies final memory permissions.
8. Releases resources when loading fails or when the image is unloaded.

The implementation creates a memory copy of the ELF layout. It does not execute the loaded program or map it at its original virtual addresses.

### Memory Loading Test Results

The test executable was a statically linked ELF64 x86-64 executable.

Results:

- Four LOAD segments were processed.
- File data matched the original ELF segment contents.
- BSS zero initialization was verified.
- BSS zero-filled region: 22,560 bytes.
- Entry point was checked against the allocated image.
- Memory was released successfully.

The allocated image size was 753,664 bytes.

---

## 7. Memory Protection

Memory protection is implemented using Linux `mprotect()`.

The loader translates ELF segment flags into memory-protection flags:

| ELF Flag | Linux Protection |
|---|---|
| `PF_R` | `PROT_READ` |
| `PF_W` | `PROT_WRITE` |
| `PF_X` | `PROT_EXEC` |

### Page-Level Permission Handling

Memory protection is applied at page granularity rather than individual byte or segment granularity.

When multiple LOAD segments share the same memory page, their permissions are combined using a bitwise OR operation. The resulting page receives the union of the permissions requested by all segments occupying that page.

This approach reflects the page-level behavior of Linux `mprotect()`. Consequently, a permission required by one segment may also apply to other bytes within the same shared page.

The implementation rejects any resulting page that would be both writable and executable (W+X).

Pages that do not belong to a LOAD segment remain `PROT_NONE`.


### 7.1 Write Protection Test

A child process attempted to write to a read-only memory page.

The operating system generated `SIGSEGV`, confirming that the write was blocked.

Result:

```text
PASS: Write to read-only page blocked (SIGSEGV)
```

### 7.2 Execute Protection Test

A child process attempted to execute instructions from a non-executable memory page.

The operating system generated `SIGSEGV`, confirming that execution was blocked.

Result:

```text
PASS: Execution from non-executable page blocked (SIGSEGV)
```

### 7.3 Memory Release Test

The allocated image was successfully released using `munmap()`.

Result:

```text
PASS: Memory released successfully
```

---

## 8. Test Summary

| Component | Result |
|---|---|
| APAGER | PASS |
| HPAGER | PASS |
| DPAGER | PASS |
| ELF Header Validation | PASS |
| ELF LOAD Segment Validation | PASS |
| ELF Memory Loading | PASS |
| BSS Zero Initialization | PASS |
| Memory Protection Application | PASS |
| Write Protection Test | PASS |
| Execute Protection Test | PASS |
| Memory Release | PASS |

---

## 9. Development Environment

The implementation and tests were developed and executed in a Linux environment under WSL.

Compiler: GCC
Programming language: C
Build standard: C99
Compiler options: `-Wall -Wextra -Wpedantic`

The current test environment used Ubuntu 26.04.1 LTS. The assignment's specified Ubuntu 24.04 environment still requires separate compatibility verification.

---

## 10. Limitations and Future Work

The current ELF loader is an educational memory-image loader, not a complete executable runtime loader.

The following capabilities are outside the current implementation:

- Loading an executable at its original virtual addresses.
- Executing the loaded ELF image.
- Setting up the process stack and initial execution state.
- Supporting PIE (`ET_DYN`) executables.
- Full dynamic linking and relocation processing.

Further work should follow the remaining laboratory requirements, including the ELF execution setup and other assigned memory-management components.

---

## 11. Conclusion

The project implements pager simulations and an ELF memory-image loader.

The ELF implementation validates ELF headers and LOAD segments, copies segment data, initializes BSS, and applies page-level memory protections.

The tests confirmed that writing to read-only pages and executing from non-executable pages are blocked by the operating system.

The implementation provides a practical foundation for understanding ELF loading and memory-protection mechanisms in Linux.