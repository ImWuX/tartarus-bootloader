#ifndef __TARTARUS_BOOTLOADER_HEADER
#define __TARTARUS_BOOTLOADER_HEADER

#include <stdint.h>

typedef uint64_t tartarus_addr_t;
typedef uint64_t tartarus_uint_t;

typedef struct {
    tartarus_addr_t paddr;
    tartarus_addr_t vaddr;
    tartarus_uint_t size;
    tartarus_addr_t entry;
} tartarus_elf_image_t;

typedef struct {
    tartarus_addr_t address;
    tartarus_uint_t size;
    tartarus_uint_t width;
    tartarus_uint_t height;
    tartarus_uint_t pitch;
} tartarus_fb_t;

typedef enum {
    TARTARUS_MEMAP_TYPE_USABLE = 0,
    TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_NVS,
    TARTARUS_MEMAP_TYPE_RESERVED,
    TARTARUS_MEMAP_TYPE_BAD
} tartarus_mmap_type_t;

typedef struct {
    tartarus_addr_t base;
    tartarus_uint_t length;
    tartarus_mmap_type_t type;
} tartarus_mmap_entry_t;

typedef struct {
    tartarus_elf_image_t kernel_image;
    tartarus_addr_t acpi_rsdp;
    tartarus_fb_t framebuffer;
    tartarus_addr_t memory_map;
    tartarus_uint_t memory_map_size;
} tartarus_boot_info_t;

#endif