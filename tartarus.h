#ifndef __TARTARUS_BOOTLOADER_HEADER
#define __TARTARUS_BOOTLOADER_HEADER

#include <stdint.h>

typedef uint64_t tartarus_paddr_t;
typedef uint64_t tartarus_uint_t;

typedef enum {
    TARTARUS_MEMAP_TYPE_USABLE = 0,
    TARTARUS_MEMAP_TYPE_ACPI_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_NVS,
    TARTARUS_MEMAP_TYPE_BAD
} tartarus_mmap_type_t;

typedef struct {
    tartarus_paddr_t base;
    tartarus_uint_t length;
    tartarus_mmap_type_t type;
} tartarus_mmap_entry_t;



#endif