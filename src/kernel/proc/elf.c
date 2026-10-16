// elf.c
// <kernel/proc.h>
//
// Loader for statically linked ELF64 executables (ET_EXEC). Every value
// from the file is checked against the image size and the user address
// range before it is used.

#include <kernel/proc.h>

#ifdef CONFIG_USERMODE

#include <kernel/arch/memlayout.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <string.h>

#define PAGE_SIZE 4096

#define ELF_MAGIC     "\x7F" "ELF"
#define ELFCLASS64    2
#define ELFDATA2LSB   1
#define ET_EXEC       2
#define EM_X86_64     62
#define PT_LOAD       1
#define PF_W          (1 << 1)

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

// Programs may use user space below the stack
#define USER_LOAD_END (USER_STACK_TOP - USER_STACK_PAGES * PAGE_SIZE)

static bool load_segment(uintptr_t space, const uint8_t *image, const elf64_phdr_t *ph)
{
    uint64_t vaddr = ph->p_vaddr;

    if (ph->p_filesz > ph->p_memsz)
        return false;
    if (vaddr < USER_START || ph->p_memsz > USER_LOAD_END - vaddr || vaddr > USER_LOAD_END)
        return false;

    uint64_t start = vaddr & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t end = (vaddr + ph->p_memsz + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t file_end = vaddr + ph->p_filesz;

    for (uint64_t page = start; page < end; page += PAGE_SIZE) {
        void *phys = pmm_alloc_page();
        if (phys == NULL)
            return false;

        uint8_t *dest = P2V(phys);
        memset(dest, 0, PAGE_SIZE);  // also the .bss part

        // Copy the file bytes that fall into this page
        uint64_t from = page > vaddr ? page : vaddr;
        uint64_t to = page + PAGE_SIZE < file_end ? page + PAGE_SIZE : file_end;
        if (from < to)
            memcpy(dest + (from - page), image + ph->p_offset + (from - vaddr), to - from);

        // Fails if two segments share a page; src/user aligns segments to pages
        if (!paging_map_user(space, page, (uintptr_t)phys, ph->p_flags & PF_W)) {
            pmm_free_page(phys);
            return false;
        }
    }
    return true;
}

uintptr_t elf_load(uintptr_t space, const uint8_t *image, size_t size, uintptr_t *image_end)
{
    if (size < sizeof(elf64_ehdr_t))
        return 0;

    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)image;
    if (memcmp(eh->e_ident, ELF_MAGIC, 4) != 0 || eh->e_ident[4] != ELFCLASS64 ||
        eh->e_ident[5] != ELFDATA2LSB || eh->e_type != ET_EXEC || eh->e_machine != EM_X86_64 ||
        eh->e_phentsize != sizeof(elf64_phdr_t))
        return 0;

    if (eh->e_phoff > size || (uint64_t)eh->e_phnum * sizeof(elf64_phdr_t) > size - eh->e_phoff)
        return 0;

    if (eh->e_entry < USER_START || eh->e_entry >= USER_LOAD_END)
        return 0;

    const elf64_phdr_t *phdrs = (const elf64_phdr_t *)(image + eh->e_phoff);
    uint64_t end = USER_START;
    for (unsigned i = 0; i < eh->e_phnum; i++) {
        const elf64_phdr_t *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD)
            continue;
        if (ph->p_offset > size || ph->p_filesz > size - ph->p_offset)
            return 0;
        if (!load_segment(space, image, ph))
            return 0;  // the caller frees the partly built space
        if (ph->p_vaddr + ph->p_memsz > end)
            end = ph->p_vaddr + ph->p_memsz;  // checked against USER_LOAD_END above
    }

    *image_end = (end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    return eh->e_entry;
}

#endif // CONFIG_USERMODE
