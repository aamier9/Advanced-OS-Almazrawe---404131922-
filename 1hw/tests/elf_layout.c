
#include <stdio.h>
#include <elf.h>
#include <stddef.h>

int main(void)
{
    printf("=== ELF64 Structure Layout ===\n");

    printf("Elf64_Ehdr size: %zu bytes\n",
           sizeof(Elf64_Ehdr));

    printf("Elf64_Phdr size: %zu bytes\n",
           sizeof(Elf64_Phdr));

    printf("\n--- ELF Header offsets ---\n");
    printf("e_entry:  %zu\n",
           offsetof(Elf64_Ehdr, e_entry));
    printf("e_phoff:  %zu\n",
           offsetof(Elf64_Ehdr, e_phoff));
    printf("e_phnum:  %zu\n",
           offsetof(Elf64_Ehdr, e_phnum));

    printf("\n--- Program Header offsets ---\n");
    printf("p_type:   %zu\n",
           offsetof(Elf64_Phdr, p_type));
    printf("p_offset: %zu\n",
           offsetof(Elf64_Phdr, p_offset));
    printf("p_vaddr:  %zu\n",
           offsetof(Elf64_Phdr, p_vaddr));
    printf("p_filesz: %zu\n",
           offsetof(Elf64_Phdr, p_filesz));
    printf("p_memsz:  %zu\n",
           offsetof(Elf64_Phdr, p_memsz));
    printf("p_flags:  %zu\n",
           offsetof(Elf64_Phdr, p_flags));

    return 0;
}
