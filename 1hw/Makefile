
# ============================================================
# Advanced Operating Systems Lab
# ELF Loading, Memory Management and Paging
# ============================================================

CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -Iinclude

# Common source files
ELF_COMMON = src/common/elf_parser.c src/common/elf_loader.c
MEMORY_COMMON = src/common/memory.c
STACK_COMMON = src/common/stack.c $(MEMORY_COMMON)

# ============================================================
# PHONY TARGETS
# ============================================================

.PHONY: all check clean
.PHONY: elf_tests memory_tests stack_tests
.PHONY: dynamic_tests demand_tests hybrid_tests
.PHONY: performance_tests malloc_hook_test

# Default target
all: check

# ============================================================
# 1. EXISTING PAGER TESTS
# ============================================================

test_run: tests/hello_local.o \
          src/apager/apager.o \
          src/hpager/hpager.o \
          src/dpager/dpager.o
	$(CC) $(CFLAGS) $^ -o test_runner

# ============================================================
# 2. ELF TESTS
# ============================================================

elf_layout_test: tests/elf_layout.c
	$(CC) $(CFLAGS) $< -o tests/elf_layout

elf_validate_test: tests/elf_validate.c $(ELF_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/elf_validate

elf_load_test: tests/elf_load_test.c $(ELF_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/elf_load_test

elf_protection_test: tests/elf_protection_test.c $(ELF_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/elf_protection_test

mmap_demo: tests/mmap_demo.c
	$(CC) $(CFLAGS) $< -o tests/mmap_demo

elf_target: tests/elf_target.c
	$(CC) $(CFLAGS) -static -no-pie $< -o tests/elf_target

elf_tests: elf_layout_test \
           elf_validate_test \
           elf_load_test \
           elf_protection_test \
           mmap_demo \
           elf_target

# ============================================================
# 3. MEMORY MANAGEMENT TESTS
# ============================================================

memory_test: tests/memory_test.c $(MEMORY_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/memory_test

memory_tests: memory_test

# ============================================================
# 4. USER STACK TESTS
# ============================================================

stack_test: tests/stack_test.c $(STACK_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/stack_test

stack_tests: stack_test

# ============================================================
# 5. DYNAMIC LINKING TESTS
# ============================================================

dynamic_linking/libmalloc_hook.so: dynamic_linking/malloc_hook.c
	$(CC) $(CFLAGS) -shared -fPIC $< -o $@ -ldl

dynamic_linking/test_malloc: dynamic_linking/test_malloc.c
	$(CC) $(CFLAGS) -fno-builtin-malloc $< -o $@

dynamic_tests: dynamic_linking/libmalloc_hook.so \
               dynamic_linking/test_malloc

malloc_hook_test: dynamic_tests
	LD_PRELOAD="$(CURDIR)/dynamic_linking/libmalloc_hook.so" ./dynamic_linking/test_malloc

# ============================================================
# 6. DEMAND PAGING TESTS
# ============================================================

demand_pager_test: tests/demand_pager_test.c \
                   src/dpager/demand_pager.c
	$(CC) $(CFLAGS) $^ -o tests/demand_pager_test

memory_error_test: tests/memory_error.c \
                   src/dpager/demand_pager.c
	$(CC) $(CFLAGS) $^ -o tests/memory_error

demand_tests: demand_pager_test memory_error_test

# ============================================================
# 7. HYBRID PAGING TESTS
# ============================================================

hybrid_pager_test: tests/hybrid_pager_test.c \
                   src/hpager/hybrid_pager.c \
                   $(ELF_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/hybrid_pager_test

hybrid_tests: hybrid_pager_test

# ============================================================
# 8. PERFORMANCE EVALUATION TESTS
# ============================================================

array_test: tests/array_test.c
	$(CC) $(CFLAGS) $< -o tests/array_test

bss_test: tests/bss_test.c
	$(CC) $(CFLAGS) $< -o tests/bss_test

sequential_access_test: tests/sequential_access.c
	$(CC) $(CFLAGS) $< -o tests/sequential_access

performance_tests: array_test \
                   bss_test \
                   sequential_access_test
	@echo "=== Array Memory Test ==="
	./tests/array_test
	@echo "=== BSS Memory Test ==="
	./tests/bss_test
	@echo "=== Sequential Access Test ==="
	./tests/sequential_access

# ============================================================
# 9. COMPLETE TEST SUITE
# ============================================================

check: test_run \
       elf_tests \
       memory_tests \
       stack_tests \
       dynamic_tests \
       demand_tests \
       hybrid_tests \
       array_test \
       bss_test \
       sequential_access_test

	@echo ""
	@echo "========================================"
	@echo "       ADVANCED OS TEST SUITE"
	@echo "========================================"

	@echo ""
	@echo "=== 1. Existing Pager Tests ==="
	./test_runner

	@echo ""
	@echo "=== 2. ELF Layout Test ==="
	./tests/elf_layout

	@echo ""
	@echo "=== 3. ELF Validation Test ==="
	./tests/elf_validate tests/elf_target

	@echo ""
	@echo "=== 4. ELF Loading Test ==="
	./tests/elf_load_test tests/elf_target

	@echo ""
	@echo "=== 5. ELF Protection Test ==="
	./tests/elf_protection_test

	@echo ""
	@echo "=== 6. Memory Management Test ==="
	./tests/memory_test

	@echo ""
	@echo "=== 7. User Stack Test ==="
	./tests/stack_test

	@echo ""
	@echo "=== 8. Dynamic Linking Test ==="
	LD_PRELOAD="$(CURDIR)/dynamic_linking/libmalloc_hook.so" ./dynamic_linking/test_malloc

	@echo ""
	@echo "=== 9. Demand Paging Test ==="
	./tests/demand_pager_test

	@echo ""
	@echo "=== 10. Invalid Memory Access Test ==="
	./tests/memory_error

	@echo ""
	@echo "=== 11. Hybrid Paging Test ==="
	./tests/hybrid_pager_test

	@echo ""
	@echo "=== 12. Array Memory Test ==="
	./tests/array_test

	@echo ""
	@echo "=== 13. BSS Memory Test ==="
	./tests/bss_test

	@echo ""
	@echo "=== 14. Sequential Access Test ==="
	./tests/sequential_access

	@echo ""
	@echo "========================================"
	@echo "       ALL TESTS COMPLETED"
	@echo "========================================"

# ============================================================
# 10. GENERIC COMPILATION RULE
# ============================================================

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# 11. CLEAN GENERATED FILES
# ============================================================

clean:
	rm -f src/*/*.o tests/*.o
	rm -f test_runner

	rm -f tests/elf_layout
	rm -f tests/elf_validate
	rm -f tests/elf_load_test
	rm -f tests/elf_protection_test
	rm -f tests/mmap_demo
	rm -f tests/elf_target

	rm -f tests/memory_test
	rm -f tests/stack_test

	rm -f tests/demand_pager_test
	rm -f tests/memory_error
	rm -f tests/hybrid_pager_test

	rm -f tests/array_test
	rm -f tests/bss_test
	rm -f tests/sequential_access

	rm -f dynamic_linking/libmalloc_hook.so
	rm -f dynamic_linking/test_malloc
