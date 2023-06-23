#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <tartarus.h>
#include <memory/vmm.h>
#include <fs/fat.h>

typedef uint64_t elf64_addr_t;
typedef uint64_t elf64_off_t;
typedef uint16_t elf64_half_t;
typedef uint32_t elf64_word_t;
typedef int32_t elf64_sword_t;
typedef uint64_t elf64_xword_t;
typedef int64_t elf64_sxword_t;

typedef struct {
    elf64_addr_t vaddr;
    elf64_addr_t paddr;
    elf64_xword_t size;
    elf64_addr_t entry;
} elf_loaded_image_t;

elf_loaded_image_t *elf_load(fat_file_t *file, vmm_address_space_t address_space);

#endif