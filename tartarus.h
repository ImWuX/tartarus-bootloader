#ifndef __TARTARUS_BOOTLOADER_HEADER
#define __TARTARUS_BOOTLOADER_HEADER

#include <stdint.h>

#define __TARTARUS_PACKED __attribute__((packed))

#ifndef __TARTARUS_ARCH_OVERRIDE
#if defined __x86_64__ || defined __i386__ || defined _M_X64 || defined _M_IX86
#ifndef __AMD64
#define __AMD64
#endif
#endif
#endif

#ifdef __TARTARUS_NO_PTR
#define __TARTARUS_PTR(TYPE) uint64_t
#else
#define __TARTARUS_PTR(TYPE) TYPE
#endif

typedef enum {
    TARTARUS_MEMORY_MAP_TYPE_USABLE,
    TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE,
    TARTARUS_MEMORY_MAP_TYPE_ACPI_RECLAIMABLE,
    TARTARUS_MEMORY_MAP_TYPE_ACPI_NVS,
    TARTARUS_MEMORY_MAP_TYPE_RESERVED,
    TARTARUS_MEMORY_MAP_TYPE_BAD
} tartarus_memory_map_type_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    tartarus_memory_map_type_t type;
} __TARTARUS_PACKED tartarus_memory_map_entry_t;

#endif