#include "vmm.h"
#include <stddef.h>
#include <memory/pmm.h>
#include <libc.h>
#include <log.h>

#define FOUR_GB 0x100000000
#define HHDM_OFFSET 0xFFFF800000000000

#ifdef __AMD64
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
        uint64_t *new_table = pmm_alloc_page();
        memset(new_table, 0, PAGE_SIZE);
        current_table[indexes[i]] = ((uint64_t) (uintptr_t) new_table & PT_ADDRESS_MASK) | PT_PRESENT | PT_RW;
        current_table = new_table;
    }
    current_table[indexes[highest_index]] = (paddr & PT_ADDRESS_MASK) | PT_PRESENT | PT_RW;
    if(large) current_table[indexes[highest_index]] |= PT_LARGE;
}
#endif

void map_region(void *map, uint64_t paddr, uint64_t vaddr, uint64_t length) {
    uint64_t offset = 0;
    while(offset < length) {
        bool large = paddr % PAGE_SIZE_LARGE == 0 && vaddr % PAGE_SIZE_LARGE == 0 && length - offset >= PAGE_SIZE_LARGE;
        map_page(map, paddr, vaddr, large);
        paddr += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
        vaddr += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
        offset += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
    }
}

#ifdef __AMD64
void *vmm_initialize() {
    void *map = pmm_alloc_page();
    memset(map, 0, PAGE_SIZE);

    // Map 2nd page to 4GB
    map_region(map, PAGE_SIZE, PAGE_SIZE, FOUR_GB - PAGE_SIZE);
    map_region(map, PAGE_SIZE, HHDM_OFFSET + PAGE_SIZE, FOUR_GB - PAGE_SIZE);

    // TODO: Map memory map entries past 4GB

#if defined __BIOS
    asm volatile("mov %0, %%cr3" : : "r" (map));
#endif
    return map;
}
#endif