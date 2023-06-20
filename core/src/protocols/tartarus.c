#include "tartarus.h"
#include <log.h>
#include <memory/heap.h>
#include <core.h>
#ifdef __UEFI
#include <efi.h>
#endif

#ifdef __AMD64
#ifdef __BIOS
extern void *protocol_tartarus_bios_handoff(uint32_t boot_info);
#endif
#ifdef __UEFI64
extern void *protocol_tartarus_uefi_handoff(uint64_t boot_info);
#endif
#endif

[[noreturn]] void protocol_tartarus_handoff(tartarus_elf_image_t *kernel, acpi_rsdp_t *rsdp, vmm_address_space_t *address_space) {
#ifdef __AMD64
    asm volatile("mov %0, %%cr3" : : "r" (address_space));
#endif
    tartarus_boot_info_t *boot_info = heap_alloc(sizeof(tartarus_boot_info_t));
    boot_info->kernel_image = *kernel;
    boot_info->acpi_rsdp = (uintptr_t) rsdp;

#ifdef __AMD64
#ifdef __BIOS
    protocol_tartarus_bios_handoff((uint32_t) boot_info);
    __builtin_unreachable();
#endif
#ifdef __UEFI
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    EFI_STATUS status = g_st->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
    if(status == EFI_BUFFER_TOO_SMALL) {
        map = heap_alloc(map_size);
        status = g_st->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
        if(EFI_ERROR(status)) log_panic("PROTOCOL/TARTARUS", "Unable retrieve the UEFI memory map");
    }
    g_st->BootServices->ExitBootServices(g_imagehandle, map_key);

    protocol_tartarus_uefi_handoff((uint64_t) boot_info);
    __builtin_unreachable();
#endif
#endif
    log_panic("PROTOCOL/TARTARUS", "Failed to handoff\n");
}