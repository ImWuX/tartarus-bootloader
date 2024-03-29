#ifndef MEMORY_VMM
#define MEMORY_VMM

#include <stdint.h>

#define VMM_FLAG_EXEC (1 << 0)
#define VMM_FLAG_WRITE (1 << 1)
#define VMM_FLAG_READ (1 << 2)

#ifdef __AMD64
typedef void *vmm_address_space_t;
#else
#error Invalid target or missing implementation
#endif

vmm_address_space_t vmm_initialize();
void vmm_map(vmm_address_space_t address_space, uint64_t paddr, uint64_t vaddr, uint64_t length, uint8_t flags);

#endif