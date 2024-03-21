#include "elf.h"
#include <log.h>
#include <lib/mem.h>
#include <lib/math.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <memory/heap.h>

#define LITTLE_ENDIAN 1
#define CLASS64 2
#define MACHINE_386 0x3E
#define TYPE_EXECUTABLE 2

#define ELF_MAGIC "\x7f" "ELF"

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7

#define ELF_ASSERT(ASSERTION, WARN_MSG) if(!(ASSERTION)) { log_warning("ELF", WARN_MSG); return NULL; }

typedef uint64_t elf64_addr_t;
typedef uint64_t elf64_off_t;
typedef uint16_t elf64_half_t;
typedef uint32_t elf64_word_t;
typedef int32_t elf64_sword_t;
typedef uint64_t elf64_xword_t;
typedef int64_t elf64_sxword_t;

typedef struct {
    uint8_t magic[4];
    uint8_t class;
    uint8_t encoding;
    uint8_t file_version;
    uint8_t abi;
    uint8_t abi_version;
    uint8_t rsv0[6];
    uint8_t nident;
} __attribute__((packed)) elf_identifier_t;

typedef struct {
    elf_identifier_t identifier;
    elf64_half_t type;
    elf64_half_t machine;
    elf64_word_t version;
    elf64_addr_t entry;
    elf64_off_t program_header_offset;
    elf64_off_t section_header_offset;
    elf64_word_t flags;
    elf64_half_t header_size;
    elf64_half_t program_header_entry_size;
    elf64_half_t program_header_entry_count;
    elf64_half_t section_header_entry_size;
    elf64_half_t section_header_entry_count;
    elf64_half_t shstrtab_index;
} __attribute__((packed)) elf64_header_t;

typedef struct {
    elf64_word_t type;
    elf64_word_t flags;
    elf64_off_t offset;
    elf64_addr_t vaddr;
    elf64_addr_t sys_v_abi_undefined;
    elf64_xword_t filesz;
    elf64_xword_t memsz;
    elf64_xword_t alignment;
} __attribute__((packed)) elf64_program_header_t;

elf_loaded_image_t *elf_load(vfs_node_t *file, void *address_space) {
    elf64_header_t header;
    if(file->ops->read(file, &header, 0, sizeof(elf64_header_t)) != sizeof(elf64_header_t)) log_panic("ELF", "Unable to read header from ELF");

    ELF_ASSERT(memcmp(header.identifier.magic, ELF_MAGIC, 4) == 0, "Invalid identififer");
#ifdef __X86_64
    ELF_ASSERT(header.machine == MACHINE_386, "Only the i386:x86-64 instruction-set is supported");
    ELF_ASSERT(header.identifier.encoding == LITTLE_ENDIAN, "Only little endian encoding is supported");
#endif
    ELF_ASSERT(header.identifier.class == CLASS64, "Only the 64bit class is supported");
    ELF_ASSERT(header.version <= 1, "Unsupported version");
    ELF_ASSERT(header.type == TYPE_EXECUTABLE, "Only executables are supported");
    ELF_ASSERT(header.program_header_offset > 0 && header.program_header_entry_count > 0, "No program headers?");

    elf64_addr_t lowest_vaddr = UINT64_MAX;
    elf64_addr_t highest_vaddr = 0;
    for(elf64_half_t i = 0; i < header.program_header_entry_count; i++) {
        elf64_program_header_t program_header;
        if(file->ops->read(file, &program_header, header.program_header_offset + header.program_header_entry_size * i, header.program_header_entry_size) != header.program_header_entry_size) {
            log_warning("ELF", "Unable to read program header %u", i);
            return NULL;
        }
        if(program_header.type != PT_LOAD || program_header.memsz == 0) continue;
        if(program_header.vaddr < lowest_vaddr) lowest_vaddr = program_header.vaddr;
        if(program_header.vaddr + program_header.memsz > highest_vaddr) highest_vaddr = program_header.vaddr + program_header.memsz;
    }

    elf64_xword_t size = highest_vaddr - lowest_vaddr;
    elf64_xword_t page_count = MATH_DIV_CEIL(size, PMM_PAGE_SIZE);
    void *paddr = pmm_alloc(PMM_AREA_MAX, page_count);
    for(elf64_half_t i = 0; i < header.program_header_entry_count; i++) {
        elf64_program_header_t program_header;
        if(file->ops->read(file, &program_header, header.program_header_offset + header.program_header_entry_size * i, header.program_header_entry_size) != header.program_header_entry_size) {
            log_warning("ELF", "Unable to read program header %u", i);
            return NULL;
        }

        void *addr = paddr + (program_header.vaddr - lowest_vaddr);
        memset(addr, 0, program_header.memsz);

        elf64_addr_t aligned_vaddr = program_header.vaddr - program_header.vaddr % PMM_PAGE_SIZE;
        elf64_xword_t aligned_size = (program_header.memsz + (program_header.vaddr % PMM_PAGE_SIZE) + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE * PMM_PAGE_SIZE;
        vmm_map(address_space, (uintptr_t) addr, aligned_vaddr, aligned_size, program_header.flags & 0b111);

        if(program_header.type != PT_LOAD) continue;
        if(file->ops->read(file, addr, program_header.offset, program_header.filesz) != program_header.filesz) {
            pmm_free(paddr, page_count);
            log_warning("ELF", "Unable to read program segment %u", i);
            return NULL;
        }
    }

    elf_loaded_image_t *image = heap_alloc(sizeof(elf_loaded_image_t));
    image->paddr = (uintptr_t) paddr;
    image->vaddr = lowest_vaddr;
    image->size = size;
    image->entry = header.entry;
    return image;
}