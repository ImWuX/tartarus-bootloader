#include "elf.h"
#include <vmm.h>
#include <pmm.h>
#include <log.h>
#include <boot/memap.h>

elf64_addr_t elf_read_file(file_descriptor_t *file_descriptor) {
    void *elf_buffer = pmm_request_page();

    if(fread(file_descriptor, sizeof(elf64_header_t), elf_buffer)) return 0;
    elf64_header_t *header = (elf64_header_t *) elf_buffer;
    elf_buffer += sizeof(elf64_header_t);

    if(
        header->identifier.file_identifier[0] != ELF_IDENTIFIER0 ||
        header->identifier.file_identifier[1] != ELF_IDENTIFIER1 ||
        header->identifier.file_identifier[2] != ELF_IDENTIFIER2 ||
        header->identifier.file_identifier[3] != ELF_IDENTIFIER3
    ) {
        log("Invalid ELF identififer");
        return 0;
    }

    if(header->identifier.architecture != ELF_ARCH_64) {
        log("Unsupported architecture (Only 64bit is supported)");
        return 0;
    }

    if(header->identifier.endian != ELF_LITTLE_ENDIAN) {
        log("Unsupported byte order");
        return 0;
    }

    if(header->machine != ELF_MACHINE_386) {
        log("Unsupported instruction set architecture (Only i386:x86-64 is supported)");
        return 0;
    }

    if(header->version != ELF_VERSION) {
        log("Unsupported ELF version");
        return 0;
    }

    if(header->type != ELF_EXECUTABLE) {
        log("Unsupported ELF file type (Only executables are supported)");
        return 0;
    }

    if(header->program_header_offset == 0 || header->program_header_entry_count <= 0) {
        log("No program headers?");
        return 0;
    }

    elf64_addr_t base_address = UINT64_MAX;
    for(int i = 0; i < header->program_header_entry_count; i++) {
        if(fseekto(file_descriptor, header->program_header_offset + header->program_header_entry_size * i)) return 0;
        if(fread(file_descriptor, header->program_header_entry_size, elf_buffer)) return 0;
        elf64_program_header_t *pheader = (elf64_program_header_t *) elf_buffer;

        if(pheader->vaddr < base_address) base_address = pheader->vaddr;
    }

    elf64_xword_t size = 0;
    for(int i = 0; i < header->program_header_entry_count; i++) {
        if(fseekto(file_descriptor, header->program_header_offset + header->program_header_entry_size * i)) return 0;
        if(fread(file_descriptor, header->program_header_entry_size, elf_buffer)) return 0;
        elf64_program_header_t *pheader = (elf64_program_header_t *) elf_buffer;

        elf64_xword_t s = pheader->vaddr - base_address + pheader->memsz;
        if(s > size) size = s;
    }

    uint64_t page_count = size / 0x1000 + (size % 0x1000 > 0 ? 1 : 0);
    void *phys_address = pmm_request_linear_pages_type(page_count, BOOT_MEMAP_TYPE_KERNEL);
    for(uint64_t i = 0; i < page_count; i++) {
        vmm_map_memory((uint64_t) (uint32_t) phys_address + i * 0x1000, base_address + i * 0x1000);
    }

    for(int i = 0; i < header->program_header_entry_count; i++) {
        if(fseekto(file_descriptor, header->program_header_offset + header->program_header_entry_size * i)) return 0;
        if(fread(file_descriptor, header->program_header_entry_size, elf_buffer)) return 0;
        elf64_program_header_t *pheader = (elf64_program_header_t *) elf_buffer;

        void *addr = phys_address + (pheader->vaddr - base_address);
        pmm_set(0, addr, pheader->memsz);

        if(fseekto(file_descriptor, pheader->offset)) return 0;
        if(fread(file_descriptor, pheader->filesz, addr)) return 0;
    }

    return header->entry;
}