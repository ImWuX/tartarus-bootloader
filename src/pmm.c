#include "pmm.h"
#include <stdbool.h>
#include <e820.h>
#include <log.h>

#define PAGE_SIZE 0x1000
#define BOOTSECTOR_BASE 0x7000
#define MAX_MEMAP_ENTRIES 255
#define LOWEST_MEMORY_BOUNDARY 0x1000

extern uint8_t ld_bootloader_end[];

tartarus_memap_entry_t *g_memap;
uint16_t g_memap_length;

// TODO: This is prob kinda wack
static uint64_t align_page_up(uint64_t unaligned_address) {
    return (unaligned_address + PAGE_SIZE) & ~(uint64_t) (PAGE_SIZE - 1);
}

// TODO: Possibly rewrite this function as I redid the other algo
static bool memap_claim(uint64_t base, uint64_t length, tartarus_memap_entry_type_t type) {
    for(uint16_t i = 0; i < g_memap_length; i++) {
        if(g_memap[i].base_address + g_memap[i].length <= base) continue;
        if(g_memap[i].type != TARTARUS_MEMAP_TYPE_USABLE) return true;
        if(g_memap[i].base_address > base) return true;
        uint64_t original_base = g_memap[i].base_address;
        uint64_t original_length = g_memap[i].length;
        tartarus_memap_entry_type_t original_type = g_memap[i].type;
        if(g_memap[i].base_address != base) {
            g_memap_length++;
            for(uint16_t j = g_memap_length - 1; j > i; j--) g_memap[j] = g_memap[j - 1];
            i++;
            g_memap[i - 1].length = base - original_base;
        }
        g_memap[i].base_address = base;
        g_memap[i].length = length;
        g_memap[i].type = type;
        if(original_base + original_length != base + length) {
            g_memap_length++;
            for(uint16_t j = g_memap_length - 1; j > i; j--) g_memap[j] = g_memap[j - 1];
            g_memap[i + 1].base_address = base + length;
            g_memap[i + 1].length = (original_base + original_length) - (base + length);
            g_memap[i + 1].type = original_type;
        }
        if(i > 0 && g_memap[i - 1].type == type && g_memap[i - 1].base_address + g_memap[i - 1].length == base) {
            g_memap[i - 1].length += length;
            g_memap_length--;
            for(uint16_t j = i; j < g_memap_length; j++) g_memap[j] = g_memap[j + 1];
            i--;
        }
        if(i < g_memap_length - 1 && g_memap[i + 1].type == type && g_memap[i + 1].base_address == base + length) {
            g_memap[i].length += g_memap[i + 1].length;
            g_memap_length--;
            for(uint16_t j = i + 1; j < g_memap_length; j++) g_memap[j] = g_memap[j + 1];
        }

        if(g_memap_length > MAX_MEMAP_ENTRIES) {
            log_panic("Memory map exceeded the entry limit");
            __builtin_unreachable();
        }
        return false;
    }
    return true;
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

        if((entry.type == TARTARUS_MEMAP_TYPE_USABLE || entry.type == TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE)) {
            if(entry.base_address % PAGE_SIZE != 0) {
                uint64_t offset = PAGE_SIZE - entry.base_address % PAGE_SIZE;
                entry.base_address += offset;
                entry.length -= offset;
            }
            if(entry.length % PAGE_SIZE != 0) {
                entry.length -= entry.length % PAGE_SIZE;
            }
        }

        g_memap[g_memap_length] = entry;
        g_memap_length++;

        if(g_memap_length > MAX_MEMAP_ENTRIES) log_panic("Memory map exceeded the entry limit");
    }

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
                    g_memap[g_memap_length].type = entry->type;
                    g_memap[g_memap_length].base_address = ot_entry_end;
                    g_memap[g_memap_length].length = entry_end - ot_entry_end;
                    e820_length++;
                    entry->length = ot_entry->base_address - entry->base_address;
                    entry_end = entry->base_address + entry->length;
                }
                continue;
            }

            if(ot_entry->base_address >= entry->base_address && ot_entry->base_address < entry_end) {
                if(ot_entry->type > entry->type) {
                    entry->length = ot_entry->base_address - entry->base_address;
                    entry_end = entry->base_address + entry->length;
                } else {
                    ot_entry->length -= entry_end - ot_entry->base_address;
                    ot_entry->base_address = entry_end;
                }
            }
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
        uint16_t smallest = i;
        for(uint16_t j = i; j < g_memap_length; j++) {
            if(g_memap[smallest].base_address > g_memap[j].base_address) smallest = j;
        }
        tartarus_memap_entry_t temp = g_memap[i];
        g_memap[i] = g_memap[smallest];
        g_memap[smallest] = temp;
    }

    memap_claim(BOOTSECTOR_BASE, align_page_up((uint32_t) (&g_memap + MAX_MEMAP_ENTRIES * sizeof(tartarus_memap_entry_t)) - BOOTSECTOR_BASE), TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE);
}

void *pmm_request_linear_pages_type(uint64_t number_of_pages, tartarus_memap_entry_type_t type) {
    uint64_t size = number_of_pages * PAGE_SIZE;
    for(uint16_t i = 0; i < g_memap_length; i++) {
        tartarus_memap_entry_t entry = g_memap[i];
        if(entry.type != TARTARUS_MEMAP_TYPE_USABLE) continue;
        if(entry.length < size) continue;
        if(entry.base_address >= UINT32_MAX) continue;
        if(memap_claim(entry.base_address, size, type)) continue;
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