#if defined __AMD64 && defined __BIOS
#include <memory/e820.h>
#endif
#ifdef __UEFI
#include <core.h>
#include <efi.h>
#endif


#if defined __AMD64 && defined __BIOS
#define E820_MAX 512

static e820_entry_t g_entries[E820_MAX];

void pmm_initialize() {
    int count = e820_load((void *) &g_entries, E820_MAX);

    for(int i = 0; i < count; i++) {
        log("Entry { Type: %i, Base: %x, Size: %x }\n", (uint64_t) g_entries[i].type, g_entries[i].address, g_entries[i].length);
    }
}
#endif

#ifdef __UEFI

void pmm_initialize() {
    EFI_STATUS status;
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    status = g_st->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
    if(status == EFI_BUFFER_TOO_SMALL) {
        status = g_st->BootServices->AllocatePool(EfiBootServicesData, map_size, (void **) &map);
        if(EFI_ERROR(status)) log_panic("Unable to allocate pool for UEFI memory map");
        status = g_st->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
        if(EFI_ERROR(status)) log_panic("Unable retrieve the UEFI memory map");
    }

    int x = 0;
    for(UINTN i = 0; i < map_size; i += descriptor_size) {
        EFI_MEMORY_DESCRIPTOR *entry = (EFI_MEMORY_DESCRIPTOR *) ((uintptr_t) map + i);
        log("Entry { Type: %i Start: %x, Size: %x  }", entry->Type, entry->PhysicalStart, entry->NumberOfPages);
        if(++x % 3 == 0) {
            log("\n");
        } else {
            log("\t");
        }
    }
}
#endif