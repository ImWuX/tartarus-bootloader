#include "pmm.h"
#include <tartarus.h>
#include <log.h>
#include <libc.h>
#if defined __AMD64 && defined __BIOS
#include <memory/e820.h>
#endif
#ifdef __UEFI
#include <core.h>
#include <efi.h>
#endif

#define IS_STRICT(type) ((type) == TARTARUS_MEMAP_TYPE_USABLE || (type) == TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE)

typedef struct {
    uintptr_t bottom_boundary;
    uintptr_t top_boundary;
} region_t;

static region_t regions[] = {
    { .bottom_boundary = 0,         .top_boundary = 0xA0000 },
    { .bottom_boundary = 0xA0000,   .top_boundary = 0x100000 },
    { .bottom_boundary = 0x100000,  .top_boundary = UINT32_MAX },
    { .bottom_boundary = 0x100000,  .top_boundary = UINTPTR_MAX }
};

uint16_t g_pmm_map_size;
tartarus_mmap_entry_t g_pmm_map[MAX_MEMAP_ENTRIES];

static void map_insert(int index, tartarus_mmap_entry_t entry) {
    if(g_pmm_map_size == MAX_MEMAP_ENTRIES) log_panic("PMM", "Memory map overflow");
    for(int i = g_pmm_map_size; i > index; i--) {
        g_pmm_map[i] = g_pmm_map[i - 1];
    }
    g_pmm_map[index] = entry;
    g_pmm_map_size++;
}

static void map_push(tartarus_mmap_entry_t entry) {
    map_insert(g_pmm_map_size, entry);
}

static void map_delete(int index) {
    for(int i = index; i < g_pmm_map_size - 1; i++) {
        g_pmm_map[i] = g_pmm_map[i + 1];
    }
    g_pmm_map_size--;
}

static int map_sorted_add(tartarus_mmap_entry_t entry) {
    int i = 0;
    for(; i < g_pmm_map_size && g_pmm_map[i].base <= entry.base; i++);
    map_insert(i, entry);
    return i;
}

// TODO: like 90% sure this is broken
static void map_sanitize() {
    restart_overlap:
    for(int i = 0; i < g_pmm_map_size; i++) {
        if(g_pmm_map[i].type != TARTARUS_MEMAP_TYPE_USABLE && g_pmm_map[i].type != TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE) continue;
        for(int j = 0; j < g_pmm_map_size; j++) {
            if(j == i) continue;
            if(g_pmm_map[j].base <= g_pmm_map[i].base && g_pmm_map[j].base + g_pmm_map[j].length >= g_pmm_map[i].base + g_pmm_map[i].length) {
                map_delete(i);
                goto restart_overlap;
            }
            if(g_pmm_map[j].base + g_pmm_map[j].length > g_pmm_map[i].base && g_pmm_map[j].base <= g_pmm_map[i].base) {
                uint64_t diff = (g_pmm_map[j].base + g_pmm_map[j].length) - g_pmm_map[i].base;
                if(diff < g_pmm_map[i].length) {
                    g_pmm_map[i].base += diff;
                    g_pmm_map[i].length -= diff;
                }
            }
            if(g_pmm_map[j].base > g_pmm_map[i].base && g_pmm_map[j].base < g_pmm_map[i].base + g_pmm_map[i].length) {
                if(g_pmm_map[i].base + g_pmm_map[i].length > g_pmm_map[j].base + g_pmm_map[j].length) {
                    map_push((tartarus_mmap_entry_t) {
                        .base = g_pmm_map[j].base + g_pmm_map[j].length,
                        .length = (g_pmm_map[i].base + g_pmm_map[i].length) - (g_pmm_map[j].base + g_pmm_map[j].length),
                        .type = g_pmm_map[i].type
                    });
                }
                g_pmm_map[i].length -= g_pmm_map[i].base + g_pmm_map[i].length - g_pmm_map[j].base;
            }
        }
        if(g_pmm_map[i].length == 0) {
            map_delete(i);
            goto restart_overlap;
        }
    }

    restart_merge:
    for(int i = 0; i < g_pmm_map_size; i++) {
        for(int j = 0; j < g_pmm_map_size; j++) {
            if(g_pmm_map[i].type != g_pmm_map[j].type) continue;
            if(g_pmm_map[i].base == g_pmm_map[j].base + g_pmm_map[j].length) {
                g_pmm_map[i].length += g_pmm_map[j].length;
                map_delete(j);
                goto restart_merge;
            }
        }
    }

    restart_align:
    for(int i = 0; i < g_pmm_map_size; i++) {
        if(g_pmm_map[i].type != TARTARUS_MEMAP_TYPE_USABLE && g_pmm_map[i].type != TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE) continue;
        uint64_t diff = PAGE_SIZE - g_pmm_map[i].base % PAGE_SIZE;
        if(diff != PAGE_SIZE) {
            if(g_pmm_map[i].length < diff || g_pmm_map[i].length - diff < PAGE_SIZE) {
                map_delete(i);
                goto restart_align;
            }
            g_pmm_map[i].base += diff;
            g_pmm_map[i].length -= diff;
        }
        g_pmm_map[i].length -= g_pmm_map[i].length % PAGE_SIZE;
    }

    for(int i = 1; i < g_pmm_map_size; i++) {
        for(int j = 0; j < g_pmm_map_size - i; j++) {
            if(g_pmm_map[j].base < g_pmm_map[j + 1].base) continue;
            tartarus_mmap_entry_t temp_entry = g_pmm_map[j];
            g_pmm_map[j] = g_pmm_map[j + 1];
            g_pmm_map[j + 1] = temp_entry;
        }
    }

    for(int i = 0; i < g_pmm_map_size; i++) {
        if(g_pmm_map[i].type != TARTARUS_MEMAP_TYPE_USABLE && g_pmm_map[i].type != TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE) continue;
        if(g_pmm_map[i].base >= PAGE_SIZE) continue;
        if(g_pmm_map[i].length <= PAGE_SIZE) {
            map_delete(i);
            break;
        } else {
            g_pmm_map[i].base += PAGE_SIZE;
            g_pmm_map[i].length -= PAGE_SIZE;
            break;
        }
    }
}

/*
 *  Converts memory from an entry type to another type
 *  Returns true if the region given is not governed by the src type (This also means it did not perform the conversion).
 */
bool pmm_convert(tartarus_mmap_type_t src_type, tartarus_mmap_type_t dest_type, uint64_t base, uint64_t length) {
    if(length == 0) log_panic("PMM", "Tried to create a zero-length entry");
    if(IS_STRICT(dest_type) && (base % PAGE_SIZE != 0 || length % PAGE_SIZE != 0)) log_panic("PMM", "Tried to convert a non-aligned region to a strict type");
    int i = 0;
    for(; i < g_pmm_map_size; i++) {
        if(g_pmm_map[i].type != src_type) continue;
        if(g_pmm_map[i].base > base || g_pmm_map[i].base + g_pmm_map[i].length < base + length) continue;
        goto found;
    }
    return true;
    found:
    uint64_t end_diff = (g_pmm_map[i].base + g_pmm_map[i].length) - (base + length);
    if(end_diff > 0) {
        int j = map_sorted_add((tartarus_mmap_entry_t) {
            .base = base + length,
            .length = end_diff,
            .type = g_pmm_map[i].type
        });
        g_pmm_map[i].length -= end_diff;
    }

    uint64_t start_diff = base - g_pmm_map[i].base;
    if(start_diff > 0) {
        g_pmm_map[i].length = start_diff;
        i = map_sorted_add((tartarus_mmap_entry_t) {
            .base = base,
            .length = length,
            .type = dest_type
        });
    } else {
        g_pmm_map[i].type = dest_type;
    }

    for(int j = 0; j < g_pmm_map_size; j++) {
        if(j == i) continue;
        if(g_pmm_map[j].type != dest_type) continue;
        if(g_pmm_map[i].base + g_pmm_map[i].length == g_pmm_map[j].base) {
            g_pmm_map[i].length += g_pmm_map[j].length;
            map_delete(j);
        }
        if(g_pmm_map[j].base + g_pmm_map[j].length == g_pmm_map[i].base) {
            g_pmm_map[j].length += g_pmm_map[i].length;
            map_delete(i);
            i = j;
        }
    }

    return false;
}

void *pmm_alloc_ext(tartarus_mmap_type_t src_type, tartarus_mmap_type_t dest_type, pmm_area_t area, size_t page_count) {
    uintptr_t area_start = regions[area].bottom_boundary;
    uintptr_t area_end = regions[area].top_boundary;

    size_t length = page_count * PAGE_SIZE;
    for(int i = 0; i < g_pmm_map_size; i++) {
        if(g_pmm_map[i].type != src_type) continue;
        if(g_pmm_map[i].base + g_pmm_map[i].length <= area_start) continue; // entry ends before area start
        if(g_pmm_map[i].base >= area_end) continue; // entry starts after area end

        uintptr_t ue_base = g_pmm_map[i].base;
        if(g_pmm_map[i].base < area_start) ue_base = area_start;
        uintptr_t ue_length = g_pmm_map[i].length - (ue_base - g_pmm_map[i].base);
        if(ue_base + ue_length > area_end) ue_length -= (ue_base + ue_length) - area_end;
        if(ue_length < length) continue; // claim does not fit inside entry

        if(pmm_convert(src_type, dest_type, ue_base, length)) log_panic("PMM", "Claim failed because area was not valid (?)");

        return (void *) ue_base;
    }
    log_panic("PMM", "Out of memory");
}

void *pmm_alloc(pmm_area_t area, size_t page_count) {
    return pmm_alloc_ext(TARTARUS_MEMAP_TYPE_USABLE, TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE, area, page_count);
}

void *pmm_alloc_page(pmm_area_t area) {
    return pmm_alloc(area, 1);
}

void pmm_free(void *address, size_t page_count) {
    if(pmm_convert(TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE, TARTARUS_MEMAP_TYPE_USABLE, (uint64_t) (uintptr_t) address, (uint64_t) page_count * PAGE_SIZE)) log_panic("PMM", "Cannot free an unallocated area");
}

void pmm_free_page(void *address) {
    pmm_free(address, 1);
}

#if defined __AMD64 && defined __BIOS
#define E820_MAX 512

void pmm_initialize() {
    g_pmm_map_size = 0;

    e820_entry_t e820[E820_MAX];
    int e820_size = e820_load((void *) &e820, E820_MAX);

    for(int i = 0; i < e820_size; i++) {
        tartarus_mmap_type_t type;
        switch(e820[i].type) {
            case E820_TYPE_USABLE:
                type = TARTARUS_MEMAP_TYPE_USABLE;
                break;
            case E820_TYPE_ACPI_RECLAIMABLE:
                type = TARTARUS_MEMAP_TYPE_ACPI_RECLAIMABLE;
                break;
            case E820_TYPE_ACPI_NVS:
                type = TARTARUS_MEMAP_TYPE_ACPI_NVS;
                break;
            case E820_TYPE_RESERVED:
            default:
                type = TARTARUS_MEMAP_TYPE_RESERVED;
                break;
            case E820_TYPE_BAD:
                type = TARTARUS_MEMAP_TYPE_BAD;
                break;
        }
        map_push((tartarus_mmap_entry_t) {
            .base = e820[i].address,
            .length = e820[i].length,
            .type = type
        });
    }

    map_sanitize();
}
#elif defined __UEFI
void pmm_initialize() {
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    EFI_STATUS status = g_st->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
    if(status == EFI_BUFFER_TOO_SMALL) {
        map_size += descriptor_size * 6;
        status = g_st->BootServices->AllocatePool(EfiBootServicesData, map_size, (void **) &map);
        if(EFI_ERROR(status)) log_panic("PMM", "Unable to allocate pool for UEFI memory map");
        status = g_st->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
        if(EFI_ERROR(status)) log_panic("PMM", "Unable retrieve the UEFI memory map");
    }

    g_pmm_map_size = 0;
    for(UINTN i = 0; i < map_size; i += descriptor_size) {
        EFI_MEMORY_DESCRIPTOR *entry = (EFI_MEMORY_DESCRIPTOR *) ((uintptr_t) map + i);
        tartarus_mmap_type_t type;
        switch(entry->Type) {
            case EfiConventionalMemory:
                type = TARTARUS_MEMAP_TYPE_USABLE;
                break;
            case EfiBootServicesCode:
            case EfiBootServicesData:
                type = TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE;
                break;
            case EfiACPIReclaimMemory:
                type = TARTARUS_MEMAP_TYPE_ACPI_RECLAIMABLE;
                break;
            case EfiACPIMemoryNVS:
                type = TARTARUS_MEMAP_TYPE_ACPI_NVS;
                break;
            case EfiLoaderCode:
            case EfiLoaderData:
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
            case EfiUnusableMemory:
            case EfiMemoryMappedIO:
            case EfiMemoryMappedIOPortSpace:
            case EfiPalCode:
            case EfiMaxMemoryType:
            case EfiReservedMemoryType:
            default:
                type = TARTARUS_MEMAP_TYPE_RESERVED;
                break;
        }

        map_push((tartarus_mmap_entry_t) {
            .base = entry->PhysicalStart,
            .length = entry->NumberOfPages * PAGE_SIZE,
            .type = type
        });
    }

    map_sanitize();

    int temp_map_size = g_pmm_map_size;
    tartarus_mmap_entry_t temp_map[MAX_MEMAP_ENTRIES];
    memcpy(temp_map, g_pmm_map, sizeof(tartarus_mmap_entry_t) * temp_map_size);

    // TODO: Leave some memory for UEFI
    for(int i = 0; i < temp_map_size; i++) {
        if(temp_map[i].type != TARTARUS_MEMAP_TYPE_USABLE) continue;
        EFI_PHYSICAL_ADDRESS address = temp_map[i].base;
        status = g_st->BootServices->AllocatePages(AllocateAddress, EfiBootServicesData, temp_map[i].length / PAGE_SIZE, &address);
        if(!EFI_ERROR(status)) continue;
        for(uintptr_t j = temp_map[i].base; j < temp_map[i].base + temp_map[i].length; j += PAGE_SIZE) {
            address = j;
            status = g_st->BootServices->AllocatePages(AllocateAddress, EfiBootServicesData, 1, &address);
            if(!EFI_ERROR(status)) continue;
            map_push((tartarus_mmap_entry_t) {
                .base = j,
                .length = PAGE_SIZE,
                .type = TARTARUS_MEMAP_TYPE_RESERVED
            });
        }
    }
}
#else
#error Invalid target or missing implementation
#endif