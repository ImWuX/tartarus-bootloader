#ifndef MEMORY_VMM
#define MEMORY_VMM

#include <stdint.h>

#ifdef __AMD64
void *vmm_initialize();
void vmm_map(void *map, uint64_t paddr, uint64_t vaddr, uint64_t length);
#endif

#endif