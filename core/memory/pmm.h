#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __ARCH_X86_64

#define PMM_GRANULARITY 0x1000

#define PMM_AREA_CONVENTIONAL ((pmm_map_area_t) {.start = 0, .end = 0xA0000})
#define PMM_AREA_LOWMEM ((pmm_map_area_t) {.start = 0, .end = 0x100000})
#define PMM_AREA_STANDARD ((pmm_map_area_t) {.start = 0x100000, .end = UINTPTR_MAX})

#else
#error Unimplemented
#endif

#define PMM_MAP_MAX_ENTRIES 1024

typedef enum {
    PMM_MAP_TYPE_FREE,
    PMM_MAP_TYPE_ALLOCATED,
    PMM_MAP_TYPE_EFI_RECLAIMABLE,
    PMM_MAP_TYPE_ACPI_RECLAIMABLE,
    PMM_MAP_TYPE_ACPI_NVS,
    PMM_MAP_TYPE_ACPI_TABLES,
    PMM_MAP_TYPE_RESERVED,
    PMM_MAP_TYPE_BAD
} pmm_map_type_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    pmm_map_type_t type;
} pmm_map_entry_t;

typedef struct {
    uint64_t start, end;
} pmm_map_area_t;

extern size_t g_pmm_map_size;
extern pmm_map_entry_t g_pmm_map[PMM_MAP_MAX_ENTRIES];

void pmm_map_set(uint64_t base, uint64_t length, pmm_map_type_t type, bool force);
void pmm_map_add(uint64_t base, uint64_t length, pmm_map_type_t type);

bool pmm_alloc_at(uint64_t address, size_t page_count, pmm_map_type_t type);
void *pmm_alloc_ext(pmm_map_area_t area, size_t page_count, size_t alignment, pmm_map_type_t type);
void *pmm_alloc(pmm_map_area_t area, size_t count);
void pmm_free(void *address, size_t count);
