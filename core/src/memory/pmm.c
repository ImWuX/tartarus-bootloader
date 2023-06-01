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

static int g_map_size;
static tartarus_mmap_entry_t g_map[MAX_MEMAP_ENTRIES];

static void map_insert(int index, tartarus_mmap_entry_t entry) {
    if(g_map_size == MAX_MEMAP_ENTRIES) log_panic("Memory map overflow");
    for(int i = g_map_size; i > index; i--) {
        g_map[i] = g_map[i - 1];
    }
    g_map[g_map_size] = entry;
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

void *pmm_alloc_page_type(tartarus_mmap_type_t type) {
    for(int i = 0; i < g_map_size; i++) {
        if(g_map[i].type != TARTARUS_MEMAP_TYPE_USABLE) continue;
        if(g_map[i].base + PAGE_SIZE > UINTPTR_MAX) continue;
        if(g_map[i].length == PAGE_SIZE) {
            g_map[i].type = type;
            return (void *) (uintptr_t) g_map[i].base;
        } else {
            uint64_t base = g_map[i].base;
            g_map[i].base += PAGE_SIZE;
            g_map[i].length -= PAGE_SIZE;

            int index = 0;
            for(; index < g_map_size; index++) {
                if(g_map[index].base > base) break;
            }

            map_insert(index, (tartarus_mmap_entry_t) {
                .base = base,
                .length = PAGE_SIZE,
                .type = type
            });
            return (void *) (uintptr_t) base;
        }
    }
    log_panic("Out of memory");
}

void *pmm_alloc_page() {
    return pmm_alloc_page_type(TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE);
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