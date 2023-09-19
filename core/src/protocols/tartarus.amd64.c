#include "tartarus.h"
#include <log.h>
#include <memory/pmm.h>
#include <memory/heap.h>
#include <config.h>
#include <core.h>
#ifdef __UEFI
#include <efi.h>
#endif

#ifdef __BIOS
extern void *protocol_tartarus_bios_handoff(uint64_t entry, void *stack, uint64_t boot_info);
#endif
#ifdef __UEFI64
extern void *protocol_tartarus_uefi_handoff(uint64_t entry, uint64_t boot_info);
#endif

#if defined __UEFI64
#define BIT64_CAST(VAL) (VAL)
#define BIT32_CAST(VAL)
#else
#define BIT64_CAST(VAL)
#define BIT32_CAST(VAL) (VAL)
#endif

[[noreturn]] void protocol_tartarus_handoff(
    uint64_t boot_info_offset,
    elf_loaded_image_t *kernel_image,
    acpi_rsdp_t *rsdp,
    vmm_address_space_t address_space,
    fb_t *framebuffer,
    tartarus_mmap_entry_t *map,
    uint16_t map_size,
    uint64_t hhdm_base,
    uint64_t hhdm_size,
    module_t *modules,
    smp_cpu_t *cpus
) {
    asm volatile("mov %0, %%cr3" : : "r" (address_space));

    uint8_t cpu_count = 0;
    for(smp_cpu_t *cpu = cpus; cpu; cpu = cpu->next) cpu_count++;

    uint8_t bsp_index = 0;
    tartarus_cpu_t *cpu_array = heap_alloc(sizeof(tartarus_cpu_t) * cpu_count);
    smp_cpu_t *cpu = cpus;
    for(uint16_t i = 0; i < cpu_count; i++, cpu = cpu->next) {
        if(cpu->is_bsp) bsp_index = i;
        if(cpu->is_bsp) cpu_array[i].wake_on_write = 0;
        else cpu_array[i].wake_on_write = BIT64_CAST(uint64_t *) ((uintptr_t) cpu->wake_on_write + boot_info_offset);
        cpu_array[i].apic_id = cpu->apic_id;
    }

    uint16_t module_count = 0;
    for(module_t *module = modules; module; module = module->next) module_count++;

    tartarus_module_t *module_array = heap_alloc(sizeof(tartarus_module_t) * module_count);
    module_t *module = modules;
    for(uint16_t i = 0; i < module_count; i++, module = module->next) {
        module_array[i].name = BIT64_CAST(char *) ((uintptr_t) module->name + boot_info_offset);
        module_array[i].paddr = module->base;
        module_array[i].size = module->size;
    }

    tartarus_boot_info_t *boot_info = heap_alloc(sizeof(tartarus_boot_info_t));
    boot_info->kernel_paddr = kernel_image->paddr;
    boot_info->kernel_vaddr = kernel_image->vaddr;
    boot_info->kernel_size = kernel_image->size;
    boot_info->acpi_rsdp = BIT64_CAST(acpi_rsdp_t *) ((uintptr_t) rsdp + boot_info_offset);
    boot_info->framebuffer.address = BIT64_CAST(void *) ((uintptr_t) framebuffer->address + boot_info_offset);
    boot_info->framebuffer.size = framebuffer->size;
    boot_info->framebuffer.width = framebuffer->width;
    boot_info->framebuffer.height = framebuffer->height;
    boot_info->framebuffer.pitch = framebuffer->pitch;
    boot_info->memory_map = BIT64_CAST(tartarus_mmap_entry_t *) ((uintptr_t) map + boot_info_offset);
    boot_info->memory_map_size = map_size;
    boot_info->hhdm_base = hhdm_base;
    boot_info->hhdm_size = hhdm_size;
    boot_info->bsp_index = bsp_index;
    boot_info->cpu_count = cpu_count;
    boot_info->cpus = BIT64_CAST(tartarus_cpu_t *) ((uintptr_t) cpu_array + boot_info_offset);
    boot_info->module_count = module_count;
    boot_info->modules = BIT64_CAST(tartarus_module_t *) ((uintptr_t) module_array + boot_info_offset);

#if defined __BIOS
    void *stack = pmm_alloc(PMM_AREA_MAX, 16) + 16 * PAGE_SIZE;
    protocol_tartarus_bios_handoff(kernel_image->entry, stack, (uintptr_t) boot_info + boot_info_offset);
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

    protocol_tartarus_uefi_handoff(kernel_image->entry, (uint64_t) boot_info + boot_info_offset);
#else
#error Invalid target or missing implementation
#endif
    __builtin_unreachable();
}

#undef BIT32_CAST
#undef BIT64_CAST