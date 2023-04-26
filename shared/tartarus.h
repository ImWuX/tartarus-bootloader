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
    TARTARUS_MEMAP_TYPE_FRAMEBUFFER,
    TARTARUS_MEMAP_TYPE_BAD,
} tartarus_memap_entry_type_t;

typedef struct {
    uint64_t base_address;
    uint64_t length;
    tartarus_memap_entry_type_t type;
} __attribute__((packed)) tartarus_memap_entry_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint64_t address;
    uint8_t memory_model;
    uint16_t pitch;
} __attribute__((packed)) tartarus_framebuffer_t;

typedef struct {
    uint8_t boot_drive;
    void *kernel_symbols;
    uint64_t kernel_symbols_size;
    tartarus_memap_entry_t *memory_map;
    uint16_t memory_map_length;
    tartarus_framebuffer_t *framebuffer;
    uint64_t hhdm_start;
    uint64_t hhdm_end;
    void *rsdp;
} __attribute__((packed)) tartarus_parameters_t;

#endif