#ifndef MEMORY_PMM_H
#define MEMORY_PMM_H

#include <tartarus.h>
#include <stddef.h>

#ifdef __AMD64
#define PAGE_SIZE 0x1000
#define PAGE_SIZE_LARGE 0x200000
#endif

typedef enum {
    PMM_AREA_CONVENTIONAL,
    PMM_AREA_UPPER,
    PMM_AREA_EXTENDED
} pmm_area_t;

void pmm_initialize();
void *pmm_claim(tartarus_mmap_type_t src_type, tartarus_mmap_type_t dest_type, pmm_area_t area, size_t page_count);
void *pmm_alloc_pages(size_t page_count, pmm_area_t area);
void *pmm_alloc_page();

void pmm_map();

#endif