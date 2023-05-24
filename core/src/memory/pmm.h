#ifndef MEMORY_PMM_H
#define MEMORY_PMM_H

void pmm_initialize();
void *pmm_alloc_page();
void *pmm_alloc_page_type(tartarus_mmap_type_t type);

#endif