#pragma once
#include <stdint.h>

// These flags are assumed to match the corresponding ELF flags
#define VMM_FLAG_EXEC (1 << 0)
#define VMM_FLAG_WRITE (1 << 1)
#define VMM_FLAG_READ (1 << 2)

void *vmm_create_address_space();
void vmm_map(void *address_space, uint64_t paddr, uint64_t vaddr, uint64_t length, uint8_t flags);