#include "tartarus.h"
#include <log.h>
#include <memory/heap.h>
#include <core.h>
#ifdef __UEFI
#include <efi.h>
#endif

#ifdef __BIOS
extern void *protocol_tartarus_bios_handoff(uint32_t boot_info);
#endif
#ifdef __UEFI64
extern void *protocol_tartarus_uefi_handoff(uint64_t boot_info);
#endif

[[noreturn]] void protocol_tartarus_handoff(
    tartarus_elf_image_t *kernel,
    acpi_rsdp_t *rsdp,
    vmm_address_space_t address_space,
    fb_t *framebuffer,
    tartarus_mmap_entry_t *map,
    int map_size
) {
    asm volatile("mov %0, %%cr3" : : "r" (address_space));

    tartarus_boot_info_t *boot_info = heap_alloc(sizeof(tartarus_boot_info_t));
    boot_info->kernel_image = *kernel;
    boot_info->acpi_rsdp = (tartarus_addr_t) (uintptr_t) rsdp;
    boot_info->framebuffer.address = (tartarus_addr_t) framebuffer->address;
    boot_info->framebuffer.size = (tartarus_uint_t) framebuffer->size;
    boot_info->framebuffer.width = (tartarus_uint_t) framebuffer->width;
    boot_info->framebuffer.height = (tartarus_uint_t) framebuffer->height;
    boot_info->framebuffer.pitch = (tartarus_uint_t) framebuffer->pitch;
    boot_info->memory_map = (tartarus_addr_t) (uintptr_t) map;
    boot_info->memory_map_size = (tartarus_uint_t) map_size;

#if defined __BIOS
    protocol_tartarus_bios_handoff((uint32_t) boot_info);
#elif defined __UEFI
    UINTN umap_size = 0;
    EFI_MEMORY_DESCRIPTOR *umap = NULL;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    EFI_STATUS status = g_st->BootServices->GetMemoryMap(&umap_size, umap, &map_key, &descriptor_size, &descriptor_version);
    if(status == EFI_BUFFER_TOO_SMALL) {
        umap = heap_alloc(umap_size);
        status = g_st->BootServices->GetMemoryMap(&umap_size, umap, &map_key, &descriptor_size, &descriptor_version);
        if(EFI_ERROR(status)) log_panic("PROTOCOL/TARTARUS", "Unable retrieve the UEFI memory map");
    }
    g_st->BootServices->ExitBootServices(g_imagehandle, map_key);

    protocol_tartarus_uefi_handoff((uint64_t) boot_info);
#else
#error Invalid target or missing implementation
#endif
    __builtin_unreachable();
}