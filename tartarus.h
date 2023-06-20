#ifndef __TARTARUS_BOOTLOADER_HEADER
#define __TARTARUS_BOOTLOADER_HEADER

#include <stdint.h>

typedef uint64_t tartarus_paddr_t;
typedef uint64_t tartarus_vaddr_t;
typedef uint64_t tartarus_uint_t;

typedef struct {
    tartarus_paddr_t paddr;
    tartarus_vaddr_t vaddr;
    tartarus_uint_t size;
    tartarus_vaddr_t entry;
} tartarus_elf_image_t;

typedef enum {
    TARTARUS_MEMAP_TYPE_USABLE = 0,
    TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_NVS,
    TARTARUS_MEMAP_TYPE_RESERVED,
    TARTARUS_MEMAP_TYPE_BAD
} tartarus_mmap_type_t;

typedef struct {
    tartarus_paddr_t base;
    tartarus_uint_t length;
    tartarus_mmap_type_t type;
} tartarus_mmap_entry_t;

typedef struct {
    tartarus_elf_image_t kernel_image;
    tartarus_paddr_t acpi_rsdp;
} tartarus_boot_info_t;

#endif