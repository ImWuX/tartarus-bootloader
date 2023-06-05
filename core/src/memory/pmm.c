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

#define MAX_MEMAP_ENTRIES 512
#define CONVENTIONAL_BOUNDARY 0xA0000
#define UPPER_BOUNDARY 0x100000

static int g_map_size;
static tartarus_mmap_entry_t g_map[MAX_MEMAP_ENTRIES];

static void map_insert(int index, tartarus_mmap_entry_t entry) {
    if(g_map_size == MAX_MEMAP_ENTRIES) log_panic("Memory map overflow");
    for(int i = g_map_size; i > index; i--) {
        g_map[i] = g_map[i - 1];
    }
    g_map[index] = entry;
    g_map_size++;
}

static void map_push(tartarus_mmap_entry_t entry) {
    map_insert(g_map_size, entry);
}

static void map_delete(int index) {
    for(int i = index; i < g_map_size; i++) {
        g_map[i] = g_map[i + 1 < MAX_MEMAP_ENTRIES ? i + 1 : i];
    }
    g_map_size--;
}

static void map_sanitize() {
    restart_overlap:
    for(int i = 0; i < g_map_size; i++) {
        if(g_map[i].type != TARTARUS_MEMAP_TYPE_USABLE && g_map[i].type != TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE) continue;
        for(int j = 0; j < g_map_size; j++) {
            if(j == i) continue;
            if(g_map[j].base <= g_map[i].base && g_map[j].base + g_map[j].length >= g_map[i].base + g_map[i].length) {
                map_delete(i);
                goto restart_overlap;
            }
            if(g_map[j].base + g_map[j].length > g_map[i].base && g_map[j].base <= g_map[i].base) {
                uint64_t diff = (g_map[j].base + g_map[j].length) - g_map[i].base;
                if(diff < g_map[i].length) {
                    g_map[i].base += diff;
                    g_map[i].length -= diff;
                }
            }
            if(g_map[j].base > g_map[i].base && g_map[j].base < g_map[i].base + g_map[i].length) {
                if(g_map[i].base + g_map[i].length > g_map[j].base + g_map[j].length) {
                    map_push((tartarus_mmap_entry_t) {
                        .base = g_map[j].base + g_map[j].length,
                        .length = (g_map[i].base + g_map[i].length) - (g_map[j].base + g_map[j].length),
                        .type = g_map[i].type
                    });
                }
                g_map[i].length -= g_map[i].base + g_map[i].length - g_map[j].base;
            }
        }
        if(g_map[i].length == 0) {
            map_delete(i);
            goto restart_overlap;
        }
    }

    restart_merge:
    for(int i = 0; i < g_map_size; i++) {
        for(int j = 0; j < g_map_size; j++) {
            if(g_map[i].type != g_map[j].type) continue;
            if(g_map[i].base == g_map[j].base + g_map[j].length) {
                g_map[i].length += g_map[j].length;
                map_delete(j);
                goto restart_merge;
            }
        }
    }

    restart_align:
    for(int i = 0; i < g_map_size; i++) {
        if(g_map[i].type != TARTARUS_MEMAP_TYPE_USABLE && g_map[i].type != TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE) continue;
        uint64_t diff = PAGE_SIZE - g_map[i].base % PAGE_SIZE;
        if(diff != PAGE_SIZE) {
            if(g_map[i].length < diff || g_map[i].length - diff < PAGE_SIZE) {
                map_delete(i);
                goto restart_align;
            }
            g_map[i].base += diff;
            g_map[i].length -= diff;
        }
        g_map[i].length -= g_map[i].length % PAGE_SIZE;
    }

    for(int i = 1; i < g_map_size; i++) {
        for(int j = 0; j < g_map_size - i; j++) {
            if(g_map[j].base < g_map[j + 1].base) continue;
            tartarus_mmap_entry_t temp_entry = g_map[j];
            g_map[j] = g_map[j + 1];
            g_map[j + 1] = temp_entry;
        }
    }

    for(int i = 0; i < g_map_size; i++) {
        if(g_map[i].type != TARTARUS_MEMAP_TYPE_USABLE && g_map[i].type != TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE) continue;
        if(g_map[i].base >= 0x1000) continue;
        if(g_map[i].length <= 0x1000) {
            map_delete(i);
            break;
        } else {
            g_map[i].base += 0x1000;
            g_map[i].length -= 0x1000;
            break;
        }
    }
}

void *pmm_claim(tartarus_mmap_type_t src_type, tartarus_mmap_type_t dest_type, pmm_area_t area, size_t page_count) {
    uintptr_t area_start;
    uintptr_t area_end;
    switch(area) {
        case PMM_AREA_CONVENTIONAL:
            area_start = 0;
            area_end = CONVENTIONAL_BOUNDARY;
            break;
        case PMM_AREA_UPPER:
            area_start = CONVENTIONAL_BOUNDARY;
            area_end = UPPER_BOUNDARY;
            break;
        case PMM_AREA_EXTENDED:
            area_start = UPPER_BOUNDARY;
            area_end = UINTPTR_MAX;
            break;
        default: log_panic("Invalid memory area");
    }

    size_t length = page_count * PAGE_SIZE;
    for(int i = 0; i < g_map_size; i++) {
        if(g_map[i].type != src_type) continue;
        if(g_map[i].base + g_map[i].length <= area_start) continue; // entry ends before area start
        if(g_map[i].base >= area_end) continue; // entry starts after area end

        uintptr_t ue_base = g_map[i].base;
        if(g_map[i].base < area_start) ue_base = area_start;
        uintptr_t ue_length = g_map[i].length - (ue_base - g_map[i].base);
        if(ue_base + ue_length > area_end) ue_length -= (ue_base + ue_length) - area_end;
        if(ue_length < length) continue; // claim does not fit inside entry

        if(g_map[i].base + g_map[i].length > ue_base + length) {
            size_t offset = (g_map[i].base + g_map[i].length) - (ue_base + length);
            map_insert(i + 1, (tartarus_mmap_entry_t) {
                .base = ue_base + length,
                .length = offset,
                .type = g_map[i].type
            });
            g_map[i].length -= offset;
        }

        if(ue_base > g_map[i].base) {
            g_map[i].length -= length;
            map_insert(i, (tartarus_mmap_entry_t) {
                .base = ue_base,
                .length = length,
                .type = dest_type
            });
        } else {
            g_map[i].type = dest_type;
        }

        restart_merge:
        for(int x = 0; x < g_map_size; x++) {
            for(int y = 0; y < g_map_size; y++) {
                if(x == y) continue;
                if(g_map[x].type != g_map[y].type) continue;
                if(g_map[x].base + g_map[x].length == g_map[y].base) {
                    g_map[x].length += g_map[y].length;
                    map_delete(y);
                    goto restart_merge;
                }
            }
        }

        return (void *) ue_base;
    }
    log_panic("Out of memory");
}

void *pmm_alloc_pages(size_t page_count, pmm_area_t area) {
    return pmm_claim(TARTARUS_MEMAP_TYPE_USABLE, TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE, area, page_count);
}

void *pmm_alloc_page() {
    return pmm_claim(TARTARUS_MEMAP_TYPE_USABLE, TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE, PMM_AREA_EXTENDED, 1);
}

void pmm_map() {
    for(int i = 0; i < g_map_size; i++) {
        log("Entry %i >> Type: %i, Base: %x, Length: %x\n", (uint64_t) i, (uint64_t) g_map[i].type, (uint64_t) g_map[i].base, (uint64_t) g_map[i].length);
    }
}

#if defined __AMD64 && defined __BIOS
#define E820_MAX 512

void pmm_initialize() {
    g_map_size = 0;

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
#endif

#ifdef __UEFI
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
        if(EFI_ERROR(status)) log_panic("Unable to allocate pool for UEFI memory map");
        status = g_st->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
        if(EFI_ERROR(status)) log_panic("Unable retrieve the UEFI memory map");
    }

    g_map_size = 0;
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

    int temp_map_size = g_map_size;
    tartarus_mmap_entry_t temp_map[MAX_MEMAP_ENTRIES];
    memcpy(temp_map, g_map, sizeof(tartarus_mmap_entry_t) * temp_map_size);

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
#endif