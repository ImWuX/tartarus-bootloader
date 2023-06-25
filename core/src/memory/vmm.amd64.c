#include "vmm.h"
#include <stddef.h>
#include <memory/pmm.h>
#include <libc.h>
#include <log.h>
#include <core.h>

#define FOUR_GB 0x100000000

#define LEVELS 4

#define PT_PRESENT (1 << 0)
#define PT_RW (1 << 1)
#define PT_LARGE (1 << 7)
#define PT_NX ((uint64_t) 1 << 63)
#define PT_ADDRESS_MASK 0x000FFFFFFFFFF000 // 4 level paging

typedef enum {
    PT_SIZE_NORMAL,
    PT_SIZE_LARGE,
    PT_SIZE_HUGE
} pt_size_t;

static void map_page(uint64_t *pml4, uint64_t paddr, uint64_t vaddr, pt_size_t size, bool rw, bool nx) {
    uint64_t indexes[LEVELS];
    vaddr >>= 3;
    for(int i = 0; i < LEVELS; i++) {
        vaddr >>= 9;
        indexes[LEVELS - 1 - i] = vaddr & 0x1FF;
    }

    uint64_t *current_table = pml4;
    int highest_index = LEVELS - (size == PT_SIZE_LARGE ? (size == PT_SIZE_HUGE ? 3 : 2) : 1);
    for(int i = 0; i < highest_index; i++) {
        if(!(current_table[indexes[i]] & PT_PRESENT)) {
            uint64_t *new_table = pmm_alloc_page(PMM_AREA_MAX);
            memset(new_table, 0, PAGE_SIZE);
            current_table[indexes[i]] = ((uint64_t) (uintptr_t) new_table & PT_ADDRESS_MASK) | PT_PRESENT;
        }
        if(rw) current_table[indexes[i]] |= PT_RW;
        if(!nx) current_table[indexes[i]] &= ~PT_NX;
        current_table = (uint64_t *) (uintptr_t) (current_table[indexes[i]] & PT_ADDRESS_MASK);
    }
    current_table[indexes[highest_index]] = (paddr & PT_ADDRESS_MASK) | PT_PRESENT;
    if(size != PT_SIZE_NORMAL) current_table[indexes[highest_index]] |= PT_LARGE;
    if(rw) current_table[indexes[highest_index]] |= PT_RW;
    if(nx) current_table[indexes[highest_index]] |= PT_NX;
}

void vmm_map(vmm_address_space_t address_space, uint64_t paddr, uint64_t vaddr, uint64_t length, uint8_t flags) {
    uint64_t offset = 0;
    while(offset < length) {
        bool large = paddr % PAGE_SIZE_LARGE == 0 && vaddr % PAGE_SIZE_LARGE == 0 && length - offset >= PAGE_SIZE_LARGE;
        if(!(flags & VMM_FLAG_READ)) log_warning("VMM", "Cannot map memory without read permissions");
        map_page(address_space, paddr, vaddr, large ? PT_SIZE_LARGE : PT_SIZE_NORMAL, flags & VMM_FLAG_WRITE, g_nx && !(flags & VMM_FLAG_EXEC));
        paddr += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
        vaddr += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
        offset += large ? PAGE_SIZE_LARGE : PAGE_SIZE;
    }
}

vmm_address_space_t vmm_initialize() {
    vmm_address_space_t map = pmm_alloc_page(PMM_AREA_MAX);
    memset(map, 0, PAGE_SIZE);

    // Map 2nd page to 4GB
    vmm_map(map, PAGE_SIZE, PAGE_SIZE, FOUR_GB - PAGE_SIZE, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);
    vmm_map(map, PAGE_SIZE, HHDM_OFFSET + PAGE_SIZE, FOUR_GB - PAGE_SIZE, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);

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
        vmm_map(map, base, base, length, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);
        vmm_map(map, base, HHDM_OFFSET + base, length, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);
    }
    return map;
}