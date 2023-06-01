#ifndef MEMORY_PMM_H
#define MEMORY_PMM_H

#include <tartarus.h>

#ifdef __AMD64
#define PAGE_SIZE 0x1000
#endif

void pmm_initialize();
void *pmm_alloc_page();
void *pmm_alloc_page_type(tartarus_mmap_type_t type);

#endif