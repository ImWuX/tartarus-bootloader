#include "vmm.h"
#include <stddef.h>
#include <memory/pmm.h>
#include <libc.h>
#include <log.h>

#define FOUR_GB 0x100000000

#define LEVELS 4

#define PT_PRESENT (1 << 0)
#define PT_RW (1 << 1)
#define PT_LARGE (1 << 7)
#define PT_ADDRESS_MASK 0x000FFFFFFFFFF000 // 4 level paging

static void map_page(uint64_t *pml4, uint64_t paddr, uint64_t vaddr, bool large) {
    uint64_t indexes[LEVELS];
    vaddr >>= 3;
    for(int i = 0; i < LEVELS; i++) {
        vaddr >>= 9;
        indexes[LEVELS - 1 - i] = vaddr & 0x1FF;
    }

    uint64_t *current_table = pml4;
    int highest_index = LEVELS - (large ? 2 : 1);
    for(int i = 0; i < highest_index; i++) {
        uint64_t entry = current_table[indexes[i]];
        if(entry & PT_PRESENT) {
            current_table = (uint64_t *) (uintptr_t) (entry & PT_ADDRESS_MASK);
            continue;
        }
        uint64_t *new_table = pmm_alloc_page(PMM_AREA_MAX);
        memset(new_table, 0, PAGE_SIZE);
        current_table[indexes[i]] = ((uint64_t) (uintptr_t) new_table & PT_ADDRESS_MASK) | PT_PRESENT | PT_RW;
        current_table = new_table;
    }
    current_table[indexes[highest_index]] = (paddr & PT_ADDRESS_MASK) | PT_PRESENT | PT_RW;
    if(large) current_table[indexes[highest_index]] |= PT_LARGE;
}

void vmm_map(vmm_address_space_t address_space, uint64_t paddr, uint64_t vaddr, uint64_t length) {
    uint64_t offset = 0;
    while(offset < length) {
        bool large = paddr % PAGE_SIZE_LARGE == 0 && vaddr % PAGE_SIZE_LARGE == 0 && length - offset >= PAGE_SIZE_LARGE;
        map_page(address_space, paddr, vaddr, large);
        paddr += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
        vaddr += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
        offset += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
    }
}

vmm_address_space_t vmm_initialize() {
    vmm_address_space_t map = pmm_alloc_page(PMM_AREA_MAX);
    memset(map, 0, PAGE_SIZE);

    // Map 2nd page to 4GB
    vmm_map(map, PAGE_SIZE, PAGE_SIZE, FOUR_GB - PAGE_SIZE);
    vmm_map(map, PAGE_SIZE, HHDM_OFFSET + PAGE_SIZE, FOUR_GB - PAGE_SIZE);

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
        vmm_map(map, base, base, length);
    }
    return map;
}