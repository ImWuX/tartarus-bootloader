#ifndef MEMORY_HHDM
#define MEMORY_HHDM

#include <stdint.h>
#include <memory/vmm.h>

#define HHDM_OFFSET 0xFFFF800000000000

uint64_t hhdm_map(vmm_address_space_t address_space, uint64_t hhdm_base);

#endif