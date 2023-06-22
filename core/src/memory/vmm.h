#ifndef MEMORY_VMM
#define MEMORY_VMM

#include <stdint.h>

#define HHDM_OFFSET 0xFFFF800000000000

#ifdef __AMD64
typedef void *vmm_address_space_t;
#else
#error Invalid target or missing implementation
#endif

vmm_address_space_t vmm_initialize();
void vmm_map(vmm_address_space_t address_space, uint64_t paddr, uint64_t vaddr, uint64_t length);

#endif