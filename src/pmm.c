#include "pmm.h"
#include <e820.h>
#include <log.h>

#define PAGE_SIZE 0x1000
#define BOOTSECTOR_BASE 0x7000
#define MAX_MEMAP_ENTRIES 255
#define LOWEST_MEMORY_BOUNDARY 0x1000

extern uint8_t ld_bootloader_end[];

tartarus_memap_entry_t *g_memap;
uint16_t g_memap_length;

static void memap_sanitize() {
    for(uint16_t i = 0; i < g_memap_length; i++) {
        tartarus_memap_entry_t *entry = g_memap + i;
        if(entry->length == 0) continue;
        uint64_t entry_end = entry->base_address + entry->length;

        for(uint16_t j = 0; j < g_memap_length; j++) {
            if(i == j) continue;
            tartarus_memap_entry_t *ot_entry = g_memap + j;
            if(ot_entry->length == 0) continue;
            uint64_t ot_entry_end = ot_entry->base_address + ot_entry->length;

            if(ot_entry->base_address >= entry->base_address && ot_entry_end <= entry_end) {
                if(ot_entry->type <= entry->type) {
                    ot_entry->length = 0;
                } else {
                    if(g_memap_length + 1 >= MAX_MEMAP_ENTRIES) {
                        log_panic("Memory map exceeded the max entry limit");
                        __builtin_unreachable();
                    }
                    g_memap[g_memap_length].type = entry->type;
                    g_memap[g_memap_length].base_address = ot_entry_end;
                    g_memap[g_memap_length].length = entry_end - ot_entry_end;
                    g_memap_length++;
                    entry->length = ot_entry->base_address - entry->base_address;
                    entry_end = entry->base_address + entry->length;
                }
                continue;
            }

            if(ot_entry->base_address >= entry->base_address && ot_entry->base_address < entry_end) {
                if(ot_entry->type > entry->type) {
                    entry->length = ot_entry->base_address - entry->base_address;
                    entry_end = entry->base_address + entry->length;
                }
                if(ot_entry->type < entry->type) {
                    ot_entry->length -= entry_end - ot_entry->base_address;
                    ot_entry->base_address = entry_end;
                }
            }
        }
    }

    for(uint16_t i = 0; i < g_memap_length; i++) {
        if(g_memap[i].length == 0) continue;
        for(uint16_t j = 0; j < g_memap_length; j++) {
            if(i == j) continue;
            if(g_memap[j].length == 0) continue;
            if(g_memap[j].base_address + g_memap[j].length != g_memap[i].base_address) continue;
            if(g_memap[j].type != g_memap[i].type) continue;
            g_memap[j].length += g_memap[i].length;
            g_memap[i].length = 0;
        }
    }

    for(uint16_t i = 0; i < g_memap_length; i++) {
        if(g_memap[i].length > 0) continue;
        for(uint16_t j = i; j < g_memap_length - 1; j++) {
            g_memap[j] = g_memap[j + 1];
        }
        g_memap_length--;
        i--;
    }

    for(uint16_t i = 0; i < g_memap_length; i++) {
        if(g_memap[i].type != TARTARUS_MEMAP_TYPE_USABLE && g_memap[i].type != TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE) continue;
        if(g_memap[i].base_address % PAGE_SIZE != 0) {
            uint64_t offset = PAGE_SIZE - g_memap[i].base_address % PAGE_SIZE;
            g_memap[i].base_address += offset;
            g_memap[i].length -= offset;
        }
        if(g_memap[i].length % PAGE_SIZE != 0) {
            g_memap[i].length -= g_memap[i].length % PAGE_SIZE;
        }
    }

    for(uint16_t i = 0; i < g_memap_length; i++) {
        uint16_t smallest = i;
        for(uint16_t j = i; j < g_memap_length; j++) {
            if(g_memap[smallest].base_address > g_memap[j].base_address) smallest = j;
        }
        tartarus_memap_entry_t temp = g_memap[i];
        g_memap[i] = g_memap[smallest];
        g_memap[smallest] = temp;
    }
}

bool pmm_memap_claim(uint64_t base, uint64_t length, tartarus_memap_entry_type_t type) {
    if(g_memap_length + 1 >= MAX_MEMAP_ENTRIES) {
        log_panic("Memory map exceeded the max entry limit");
        __builtin_unreachable();
    }

    if(base % PAGE_SIZE != 0 || length % PAGE_SIZE != 0) return true;

    uint64_t end = base + length;
    for(uint16_t i = 0; i < g_memap_length; i++) {
        tartarus_memap_entry_t entry = g_memap[i];
        uint64_t entry_end = g_memap[i].base_address + g_memap[i].length;
        if(entry_end <= base) continue;
        if(entry.base_address >= end) break;
        if(entry.type > type) return true;
    }
    g_memap[g_memap_length].base_address = base;
    g_memap[g_memap_length].length = length;
    g_memap[g_memap_length].type = type;
    g_memap_length++;

    memap_sanitize();
    return false;
}

void pmm_initialize() {
    uint16_t e820_length = *(uint16_t *) E820_ADDRESS;
    e820_entry_t *e820 = (e820_entry_t *) (E820_ADDRESS + 2);

    g_memap = (tartarus_memap_entry_t *) ld_bootloader_end;
    g_memap_length = 0;
    for(uint16_t i = 0; i < e820_length; i++) {
        tartarus_memap_entry_t entry;
        entry.base_address = e820[i].address;
        entry.length = e820[i].length;
        switch(e820[i].type) {
            case E820_TYPE_USABLE:
                entry.type = TARTARUS_MEMAP_TYPE_USABLE;
                break;
            case E820_TYPE_BAD:
                entry.type = TARTARUS_MEMAP_TYPE_BAD;
                break;
            case E820_TYPE_ACPI_RECLAIMABLE:
                entry.type = TARTARUS_MEMAP_TYPE_ACPI_RECLAIMABLE;
                break;
            case E820_TYPE_ACPI_NVS:
                entry.type = TARTARUS_MEMAP_TYPE_ACPI_NVS;
                break;
            default:
                entry.type = TARTARUS_MEMAP_TYPE_RESERVED;
                break;
        }

        if(entry.base_address < LOWEST_MEMORY_BOUNDARY) {
            if(entry.length <= LOWEST_MEMORY_BOUNDARY) continue;
            entry.length -= LOWEST_MEMORY_BOUNDARY - entry.base_address;
            entry.base_address = LOWEST_MEMORY_BOUNDARY;
        }

        g_memap[g_memap_length] = entry;
        g_memap_length++;

        if(g_memap_length > MAX_MEMAP_ENTRIES) log_panic("Memory map exceeded the entry limit");
    }
    memap_sanitize();

    uint32_t size = (uint32_t) &g_memap + MAX_MEMAP_ENTRIES * sizeof(tartarus_memap_entry_t) - BOOTSECTOR_BASE;
    if(size % PAGE_SIZE != 0) size += PAGE_SIZE - size % PAGE_SIZE;
    if(pmm_memap_claim(BOOTSECTOR_BASE, size, TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE)) {
        log_panic("Failed to claim bootloaders memory range");
        __builtin_unreachable();
    }
}

void *pmm_request_linear_pages_type(uint64_t number_of_pages, tartarus_memap_entry_type_t type) {
    uint64_t size = number_of_pages * PAGE_SIZE;
    for(uint16_t i = 0; i < g_memap_length; i++) {
        tartarus_memap_entry_t entry = g_memap[i];
        if(entry.type != TARTARUS_MEMAP_TYPE_USABLE) continue;
        if(entry.length < size) continue;
        if(entry.base_address >= UINT32_MAX) continue;
        if(pmm_memap_claim(entry.base_address, size, type)) continue;
        return (void *) (uint32_t) entry.base_address;
    }
    log_panic("Out of 32bit addressable memory");
    __builtin_unreachable();
}

void *pmm_request_linear_pages(uint64_t number_of_pages) {
    return pmm_request_linear_pages_type(number_of_pages, TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE);
}

void *pmm_request_page() {
    return pmm_request_linear_pages(1);
}

void pmm_cpy(void *src, void *dest, int nbytes) {
    for(int i = 0; i < nbytes; i++)
        *((uint8_t *) dest + i) = *((uint8_t *) src + i);
}

void pmm_set(uint8_t value, void *dest, int len) {
    uint8_t *temp = (uint8_t *) dest;
    for(; len != 0; len--) *temp++ = value;
}