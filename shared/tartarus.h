#ifndef ___TARTARUS_HEADER
#define ___TARTARUS_HEADER

#include <stdint.h>

typedef enum {
    TARTARUS_MEMAP_TYPE_USABLE = 0,
    TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_NVS,
    TARTARUS_MEMAP_TYPE_KERNEL,
    TARTARUS_MEMAP_TYPE_RESERVED,
    TARTARUS_MEMAP_TYPE_BAD,
} tartarus_memap_entry_type_t;

typedef struct {
    uint64_t base_address;
    uint64_t length;
    tartarus_memap_entry_type_t type;
} tartarus_memap_entry_t;

typedef struct {
    uint8_t boot_drive;
    tartarus_memap_entry_t *memory_map;
    uint64_t memory_map_length;
    uint64_t vbe_mode_info_address;
} __attribute__((packed)) tartarus_parameters_t;

#endif