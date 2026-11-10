
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -Iinclude

ELF_COMMON = src/common/elf_parser.c src/common/elf_loader.c

.PHONY: all test_run elf_tests clean

all: test_run

# Existing pager tests
test_run: tests/hello_local.o src/apager/apager.o src/hpager/hpager.o src/dpager/dpager.o
	$(CC) $(CFLAGS) $^ -o test_runner

# ELF structure layout test
elf_layout_test: tests/elf_layout.c
	$(CC) $(CFLAGS) $< -o tests/elf_layout

# ELF validation test
elf_validate_test: tests/elf_validate.c $(ELF_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/elf_validate

# ELF memory loading test
elf_load_test: tests/elf_load_test.c $(ELF_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/elf_load_test

# ELF memory protection test
elf_protection_test: tests/elf_protection_test.c $(ELF_COMMON)
	$(CC) $(CFLAGS) $^ -o tests/elf_protection_test

# mmap demonstration
mmap_demo: tests/mmap_demo.c
	$(CC) $(CFLAGS) $< -o tests/mmap_demo

# Static ELF test executable
elf_target: tests/elf_target.c
	$(CC) $(CFLAGS) -static -no-pie $< -o tests/elf_target

# Build all ELF test programs
elf_tests: elf_layout_test elf_validate_test elf_load_test elf_protection_test mmap_demo elf_target

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*/*.o tests/*.o
	rm -f tests/elf_layout tests/elf_validate tests/elf_load_test
	rm -f tests/elf_protection_test tests/mmap_demo tests/elf_target
