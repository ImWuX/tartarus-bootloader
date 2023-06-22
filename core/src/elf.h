#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <tartarus.h>
#include <memory/vmm.h>
#include <fs/fat.h>

tartarus_elf_image_t *elf_load(fat_file_t *file, vmm_address_space_t address_space);

#endif