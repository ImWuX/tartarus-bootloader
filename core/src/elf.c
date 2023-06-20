#include "elf.h"
#include <log.h>
#include <libc.h>
#include <memory/vmm.h>
#include <memory/pmm.h>
#include <memory/heap.h>

#define ID0 0x7F
#define ID1 'E'
#define ID2 'L'
#define ID3 'F'

#define LITTLE_ENDIAN 1
#define CLASS64 2
#define MACHINE_386 0x3E
#define TYPE_EXECUTABLE 2

#define XINDEX 0xFFFF

typedef uint64_t elf64_addr_t;
typedef uint64_t elf64_off_t;
typedef uint16_t elf64_half_t;
typedef uint32_t elf64_word_t;
typedef int32_t elf64_sword_t;
typedef uint64_t elf64_xword_t;
typedef int64_t elf64_sxword_t;

typedef struct {
    uint8_t file_identifier[4];
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

typedef enum {
    PT_NULL_SEGMENT = 0,
    PT_LOAD,
    PT_DYNAMIC,
    PT_INTERP,
    PT_NOTE,
    PT_SHLIB,
    PT_PHDR,
    PT_TLS
} segment_types_t;

tartarus_elf_image_t *elf_load(fat_file_t *file, vmm_address_space_t *address_space) {
    elf64_header_t *header = heap_alloc(sizeof(elf64_header_t));
    if(fat_read(file, 0, sizeof(elf64_header_t), header) != sizeof(elf64_header_t)) log_panic("ELF", "Unable to read header from ELF");

    if(
        header->identifier.file_identifier[0] != ID0 ||
        header->identifier.file_identifier[1] != ID1 ||
        header->identifier.file_identifier[2] != ID2 ||
        header->identifier.file_identifier[3] != ID3
    ) {
        log_warning("ELF", "Invalid identififer\n");
        goto fail_header;
    }

#ifdef __AMD64
    if(header->identifier.class != CLASS64) {
        log_warning("ELF", "[AMD64] Only the 64bit class is supported\n");
        goto fail_header;
    }

    if(header->machine != MACHINE_386) {
        log_warning("ELF", "[AMD64] Only the i386:x86-64 instruction-set is supported\n");
        goto fail_header;
    }

    if(header->identifier.encoding != LITTLE_ENDIAN) {
        log_warning("ELF", "[AMD64] Only little endian encoding is supported\n");
        goto fail_header;
    }
#endif

    if(header->version > 1) {
        log_warning("ELF", "Unsupported version\n");
        goto fail_header;
    }

    if(header->type != TYPE_EXECUTABLE) {
        log_warning("ELF", "Only executables are supported\n");
        goto fail_header;
    }

    if(header->program_header_offset == 0 || header->program_header_entry_count <= 0) {
        log_warning("ELF", "No program headers?\n");
        goto fail_header;
    }

    elf64_program_header_t *program_header = heap_alloc(sizeof(elf64_program_header_t));

    elf64_addr_t base_address = UINT64_MAX;
    for(int i = 0; i < header->program_header_entry_count; i++) {
        if(fat_read(file, header->program_header_offset + header->program_header_entry_size * i, header->program_header_entry_size, program_header) != header->program_header_entry_size) {
            log_warning("ELF", "Unable to read program header %i while calculating base address\n", (uint64_t) i);
            goto fail_pheader;
        }
        if(program_header->vaddr < base_address) base_address = program_header->vaddr;
    }

    elf64_xword_t size = 0;
    for(int i = 0; i < header->program_header_entry_count; i++) {
        if(fat_read(file, header->program_header_offset + header->program_header_entry_size * i, header->program_header_entry_size, program_header) != header->program_header_entry_size) {
            log_warning("ELF", "Unable to read program header %i while calculating size\n", (uint64_t) i);
            goto fail_pheader;
        }
        elf64_xword_t s = program_header->vaddr - base_address + program_header->memsz;
        if(s > size) size = s;
    }

    elf64_xword_t page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *paddr = pmm_alloc(PMM_AREA_MAX, page_count);
    // TODO: Map segments with proper rights, allocate them pages away to do this, yada yada
    vmm_map(address_space, (uint64_t) (uintptr_t) paddr, base_address, page_count * PAGE_SIZE);

    for(int i = 0; i < header->program_header_entry_count; i++) {
        if(fat_read(file, header->program_header_offset + header->program_header_entry_size * i, header->program_header_entry_size, program_header) != header->program_header_entry_size) {
            log_warning("ELF", "Unable to read program header %i while loading\n", (uint64_t) i);
            goto fail_pheader;
        }
        void *addr = paddr + (program_header->vaddr - base_address);
        memset(addr, 0, program_header->memsz);

        if(fat_read(file, program_header->offset, program_header->filesz, addr) != program_header->filesz) {
            log_warning("ELF", "Unable to read program segment %i\n", (uint64_t) i);
            goto fail_pheader;
        }
    }

    tartarus_elf_image_t *image = heap_alloc(sizeof(tartarus_elf_image_t));
    image->paddr = (tartarus_paddr_t) (uintptr_t) paddr;
    image->vaddr = (tartarus_vaddr_t) base_address;
    image->size = (tartarus_uint_t) size;
    image->entry = (tartarus_vaddr_t) header->entry;

    heap_free(program_header);
    heap_free(header);
    return image;

    fail_pheader:
    heap_free(program_header);
    fail_header:
    heap_free(header);
    return 0;
}