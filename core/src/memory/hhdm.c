#include "hhdm.h"
#include <memory/pmm.h>

#define FOUR_GB 0x100000000

uint64_t hhdm_map(vmm_address_space_t address_space, uint64_t hhdm_base) {
    vmm_map(address_space, PAGE_SIZE, PAGE_SIZE, FOUR_GB - PAGE_SIZE, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);
    vmm_map(address_space, PAGE_SIZE, hhdm_base + PAGE_SIZE, FOUR_GB - PAGE_SIZE, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);

    uint64_t size = FOUR_GB;
    for(int i = 0; i < g_pmm_map_size; i++) {
        if(g_pmm_map[i].base + g_pmm_map[i].length < FOUR_GB) continue;
        uint64_t base = g_pmm_map[i].base;
        uint64_t length = g_pmm_map[i].length;
        if(base < FOUR_GB) {
            length -= FOUR_GB - base;
            base = FOUR_GB;
        }
        if(base % PAGE_SIZE != 0) {
            length += base % PAGE_SIZE;
            base -= base % PAGE_SIZE;
        }
        if(length % PAGE_SIZE != 0) {
            length += PAGE_SIZE - length % PAGE_SIZE;
        }
        if(base + length > size) size = base + length;
        vmm_map(address_space, base, base, length, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);
        vmm_map(address_space, base, hhdm_base + base, length, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);
    }
    return size;
}