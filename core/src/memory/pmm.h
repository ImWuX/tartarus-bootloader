#ifndef MEMORY_PMM_H
#define MEMORY_PMM_H

#include <tartarus.h>
#include <stddef.h>

#ifdef __AMD64
#define PAGE_SIZE 0x1000
#define PAGE_SIZE_LARGE 0x200000
#else
#error Invalid target or missing implementation
#endif
#define MAX_MEMAP_ENTRIES 512

typedef enum {
    PMM_AREA_CONVENTIONAL,
    PMM_AREA_UPPER,
    PMM_AREA_EXTENDED,
    PMM_AREA_MAX
} pmm_area_t;

extern uint16_t g_pmm_map_size;
extern tartarus_mmap_entry_t g_pmm_map[MAX_MEMAP_ENTRIES];

void pmm_initialize();
bool pmm_convert(tartarus_mmap_type_t src_type, tartarus_mmap_type_t dest_type, uint64_t base, uint64_t length);
void *pmm_alloc_ext(tartarus_mmap_type_t src_type, tartarus_mmap_type_t dest_type, pmm_area_t area, size_t page_count);
void *pmm_alloc(pmm_area_t area, size_t page_count);
void *pmm_alloc_page(pmm_area_t area);
void pmm_free(void *address, size_t page_count);
void pmm_free_page(void *address);

#endif