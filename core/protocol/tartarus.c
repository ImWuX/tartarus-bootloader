#include "arch/acpi.h"
#include "arch/fb.h"
#include "arch/ptm.h"
#include "arch/smp.h"
#include "arch/time.h"
#include "common/config.h"
#include "common/elf.h"
#include "common/log.h"
#include "common/panic.h"
#include "fs/vfs.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "memory/pmm.h"

#include <stddef.h>
#include <stdint.h>
#include <tartarus.h>

#ifdef __UEFI
#include "arch/uefi/uefi.h"
#endif

#define MAJOR_VERSION 3
#define MINOR_VERSION 0

#define BSP_STACK_PGCNT 16
#define AP_STACK_PGCNT 4

#define HHDM_OFFSET 0xFFFF800000000000
#define HHDM_CAST(TYPE, ADDRESS) ((__TARTARUS_PTR(TYPE))((uint64_t) (uintptr_t) (ADDRESS) + HHDM_OFFSET))

[[noreturn]] extern void x86_64_protocol_tartarus_handoff(uint64_t entry, __TARTARUS_PTR(void *) stack, uint64_t top_page_table, uint64_t boot_info, uint16_t version);

[[noreturn]] void protocol_tartarus(config_t *config, vfs_node_t *kernel_node, fb_t *fb) {
    log(LOG_LEVEL_INFO, "Tartarus Protocol Version %u.%u", MAJOR_VERSION, MINOR_VERSION);

    ptm_address_space_t *address_space = arch_ptm_create_address_space();

    // Find ACPI
    acpi_rsdp_t *rsdp = NULL;
    if(config_find_bool(config, "find_rsdp", true)) {
        rsdp = arch_acpi_find_rsdp();
        if(rsdp == NULL) log(LOG_LEVEL_WARN, "could not locate ACPI RSDP");
        arch_acpi_map_tables(rsdp);
    }
    log(LOG_LEVEL_INFO, "RSDP found at %#lx", (uintptr_t) rsdp);

    // Freeze the memory map
    size_t frozen_map_size = g_pmm_map_size;
    pmm_map_entry_t *frozen_map = heap_alloc(sizeof(pmm_map_entry_t) * frozen_map_size);
    memcpy(frozen_map, &g_pmm_map, sizeof(pmm_map_entry_t) * frozen_map_size);

    // Setup HHDM
    log(LOG_LEVEL_INFO, "Mapping HHDM");
    uint64_t hhdm_size = 0;
    for(size_t i = 0; i < frozen_map_size; i++) {
        switch(frozen_map[i].type) {
            case PMM_MAP_TYPE_FREE:
            case PMM_MAP_TYPE_ACPI_TABLES:
            case PMM_MAP_TYPE_ALLOCATED:
            case PMM_MAP_TYPE_EFI_RECLAIMABLE:
            case PMM_MAP_TYPE_ACPI_RECLAIMABLE: break;
            default:                            continue;
        }

        uint64_t base = frozen_map[i].base;
        uint64_t length = frozen_map[i].length;
        if(base % PTM_PAGE_GRANULARITY != 0) {
            length += base % PTM_PAGE_GRANULARITY;
            base -= base % PTM_PAGE_GRANULARITY;
        }
        if(length % PTM_PAGE_GRANULARITY != 0) length += PTM_PAGE_GRANULARITY - length % PTM_PAGE_GRANULARITY;

        if(base + length > hhdm_size) hhdm_size = base + length;

        arch_ptm_map(address_space, base, base, length, PTM_FLAG_READ | PTM_FLAG_WRITE | PTM_FLAG_EXEC);
        arch_ptm_map(address_space, base, HHDM_OFFSET + base, length, PTM_FLAG_READ | PTM_FLAG_WRITE);
    }
    log(LOG_LEVEL_INFO, "HHDM mapped at offset %#llx (of size %#llx)", HHDM_OFFSET, hhdm_size);

    heap_free(frozen_map);

    // Map the framebuffer into the HHDM
    if(fb != NULL) {
        log(LOG_LEVEL_DEBUG,
            "Mapping framebuffer %#llx -> %#llx [%#llx]",
            MATH_FLOOR(fb->address, PTM_PAGE_GRANULARITY),
            (uint64_t) MATH_FLOOR(fb->address, PTM_PAGE_GRANULARITY) + MATH_CEIL(fb->size, PTM_PAGE_GRANULARITY),
            (uint64_t) MATH_CEIL(fb->size, PTM_PAGE_GRANULARITY));

        arch_ptm_map(
            address_space,
            (uint64_t) (uintptr_t) MATH_FLOOR(fb->address, PTM_PAGE_GRANULARITY),
            (uint64_t) ((uintptr_t) MATH_FLOOR(fb->address, PTM_PAGE_GRANULARITY)) + HHDM_OFFSET,
            MATH_CEIL(fb->size, PTM_PAGE_GRANULARITY),
            PTM_FLAG_READ | PTM_FLAG_WRITE
        );
    }

    // Load kernel
    log(LOG_LEVEL_INFO, "Loading kernel");
    elf_loaded_image_t *kernel = elf_load(kernel_node, address_space);
    if(kernel == NULL) panic("failed to load kernel");
    log(LOG_LEVEL_INFO, "Kernel loaded (entry=%#llx)", kernel->entry);

    // Load modules
    size_t module_count = config_key_count(config, "module", CONFIG_ENTRY_TYPE_STRING);
    tartarus_module_t *modules = heap_alloc(sizeof(tartarus_module_t) * module_count);
    for(size_t i = 0, j = 0; j < module_count; i++) {
        const char *module_path = config_find_string_at(config, "module", NULL, i);
        if(module_path == NULL) {
        skip_module:
            modules = heap_realloc(modules, sizeof(tartarus_module_t) * --module_count);
            continue;
        }

        vfs_node_t *module_node = vfs_lookup(kernel_node->vfs, module_path);
        if(module_node == NULL) {
            log(LOG_LEVEL_WARN, "Module %s not found", module_path);
            goto skip_module;
        }

        size_t module_size = module_node->ops->get_size(module_node);
        void *module_addr = pmm_alloc(PMM_AREA_STANDARD, MATH_DIV_CEIL(module_size, PMM_GRANULARITY));
        if(module_node->ops->read(module_node, module_addr, 0, module_size) != module_size) {
            pmm_free(module_addr, MATH_DIV_CEIL(module_size, PMM_GRANULARITY));
            log(LOG_LEVEL_WARN, "failed to load module %s", module_path);
            goto skip_module;
        }

        char *module_name = heap_alloc(string_length(module_path) + 1);
        string_copy(module_name, module_path);
        modules[j].name = HHDM_CAST(char *, module_name);
        modules[j].paddr = (uint64_t) (uintptr_t) module_addr;
        modules[j].size = module_size;
        j += 1;

        log(LOG_LEVEL_INFO, "Loaded module %s at %#lx (of size %#lx)", module_path, (uintptr_t) module_addr, module_size);
    }

    // Allocate stack
    void *stack = pmm_alloc(PMM_AREA_STANDARD, BSP_STACK_PGCNT) + (BSP_STACK_PGCNT * PMM_GRANULARITY);


    // Prepare SMP init
#if defined(__UEFI)
    log(LOG_LEVEL_INFO, "Exiting UEFI bootservices");
    uefi_bootservices_exit();
#endif

    // Initialize SMP
    smp_cpu_t *cpus = NULL;
    if(config_find_bool(config, "smp", true)) {
        cpus = smp_initialize_aps(rsdp, address_space, AP_STACK_PGCNT, HHDM_OFFSET);
        log(LOG_LEVEL_INFO, "Initialized SMP");
    }

    // Setup boot info
    tartarus_kernel_segment_t *kernel_segments = heap_alloc(sizeof(tartarus_kernel_segment_t) * kernel->count);
    for(size_t i = 0; i < kernel->count; i++) {
        uint8_t flags = 0;
        if(kernel->regions[i]->read) flags |= TARTARUS_KERNEL_SEGMENT_FLAG_READ;
        if(kernel->regions[i]->write) flags |= TARTARUS_KERNEL_SEGMENT_FLAG_WRITE;
        if(kernel->regions[i]->execute) flags |= TARTARUS_KERNEL_SEGMENT_FLAG_EXECUTE;

        kernel_segments[i].flags = flags;
        kernel_segments[i].paddr = kernel->paddr + (kernel->regions[i]->aligned_vaddr - kernel->aligned_vaddr);
        kernel_segments[i].vaddr = kernel->regions[i]->aligned_vaddr;
        kernel_segments[i].size = kernel->regions[i]->aligned_size;
    }

    tartarus_framebuffer_t *framebuffer = NULL;
    if(fb != NULL) {
        framebuffer = heap_alloc(sizeof(tartarus_framebuffer_t));
        framebuffer->vaddr = HHDM_CAST(void *, fb->address);
        framebuffer->paddr = fb->address;
        framebuffer->size = fb->size;
        framebuffer->width = fb->width;
        framebuffer->height = fb->height;
        framebuffer->pitch = fb->pitch;
        framebuffer->bpp = fb->bpp;
        framebuffer->mask.red_position = fb->mask_red_position;
        framebuffer->mask.red_size = fb->mask_red_size;
        framebuffer->mask.green_position = fb->mask_green_position;
        framebuffer->mask.green_size = fb->mask_green_size;
        framebuffer->mask.blue_position = fb->mask_blue_position;
        framebuffer->mask.blue_size = fb->mask_blue_size;
    }

    tartarus_boot_info_t *boot_info = heap_alloc(sizeof(tartarus_boot_info_t));
    boot_info->acpi_rsdp_address = (tartarus_paddr_t) (uintptr_t) rsdp;
    boot_info->bsp_entry_stack_size = BSP_STACK_PGCNT * PMM_GRANULARITY;
    boot_info->ap_entry_stack_size = AP_STACK_PGCNT * PMM_GRANULARITY;
    boot_info->hhdm_offset = HHDM_OFFSET;
    boot_info->hhdm_size = hhdm_size;
    boot_info->kernel_segment_count = kernel->count;
    boot_info->kernel_segments = HHDM_CAST(tartarus_kernel_segment_t *, kernel_segments);
    boot_info->framebuffer_count = framebuffer != NULL ? 1 : 0;
    boot_info->framebuffers = HHDM_CAST(tartarus_framebuffer_t *, framebuffer);
    boot_info->module_count = module_count;
    boot_info->modules = HHDM_CAST(tartarus_module_t *, modules);

    if(cpus != NULL) {
        uint8_t cpu_count = 0;
        for(smp_cpu_t *cpu = cpus; cpu; cpu = cpu->next) cpu_count++;

        tartarus_cpu_t *cpu_array = heap_alloc(sizeof(tartarus_cpu_t) * cpu_count);
        smp_cpu_t *cpu = cpus;
        for(uint16_t i = 0; i < cpu_count; i++, cpu = cpu->next) {
            cpu_array[i].flags = 0;
            cpu_array[i].park_address = (__TARTARUS_PTR(tartarus_vaddr_t *)) 0;
            cpu_array[i].argument = (__TARTARUS_PTR(uint64_t *)) 0;

            if(cpu->init_failed) continue;
            cpu_array[i].flags |= TARTARUS_CPU_FLAG_BOOT_OK;

            if(cpu->is_bsp) {
                cpu_array[i].flags |= TARTARUS_CPU_FLAG_IS_BSP;
                continue;
            };

            cpu_array[i].park_address = HHDM_CAST(uint64_t *, cpu->park_address);
            cpu_array[i].argument = HHDM_CAST(uint64_t *, (uintptr_t) cpu->park_address + 8);
        }

        boot_info->cpu_count = cpu_count;
        boot_info->cpus = HHDM_CAST(tartarus_cpu_t *, cpu_array);
    } else {
        tartarus_cpu_t *bsp_cpu = heap_alloc(sizeof(tartarus_cpu_t));
        bsp_cpu->flags = TARTARUS_CPU_FLAG_BOOT_OK | TARTARUS_CPU_FLAG_IS_BSP;
        bsp_cpu->park_address = (__TARTARUS_PTR(tartarus_vaddr_t *)) 0;

        boot_info->cpu_count = 1;
        boot_info->cpus = HHDM_CAST(tartarus_cpu_t *, bsp_cpu);
    }

    // Create the elysium memory map
    tartarus_mm_entry_t *memory_map_entries = heap_alloc(sizeof(tartarus_mm_entry_t) * g_pmm_map_size);
    for(uint64_t i = 0; i < g_pmm_map_size; i++) {
        tartarus_mm_type_t type;
        switch(g_pmm_map[i].type) {
            case PMM_MAP_TYPE_FREE:             type = TARTARUS_MM_TYPE_USABLE; break;
            case PMM_MAP_TYPE_ACPI_TABLES:      type = TARTARUS_MM_TYPE_ACPI_TABLES; break;
            case PMM_MAP_TYPE_ALLOCATED:        type = TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE; break;
            case PMM_MAP_TYPE_EFI_RECLAIMABLE:  type = TARTARUS_MM_TYPE_EFI_RECLAIMABLE; break;
            case PMM_MAP_TYPE_ACPI_RECLAIMABLE: type = TARTARUS_MM_TYPE_ACPI_RECLAIMABLE; break;
            case PMM_MAP_TYPE_ACPI_NVS:         type = TARTARUS_MM_TYPE_ACPI_NVS; break;
            case PMM_MAP_TYPE_BAD:              type = TARTARUS_MM_TYPE_BAD; break;
            case PMM_MAP_TYPE_RESERVED:
            default:                            type = TARTARUS_MM_TYPE_RESERVED; break;
        }

        memory_map_entries[i].type = type;
        memory_map_entries[i].base = g_pmm_map[i].base;
        memory_map_entries[i].length = g_pmm_map[i].length;
    }

    // Fill in the final fields before handoff
    boot_info->mm_entry_count = g_pmm_map_size;
    boot_info->mm_entries = HHDM_CAST(tartarus_mm_entry_t *, memory_map_entries);
    boot_info->boot_timestamp = arch_time();

    // Handoff
    log(LOG_LEVEL_INFO, "Kernel handoff");
    x86_64_protocol_tartarus_handoff(
        kernel->entry,
        HHDM_CAST(void *, stack),
        (uintptr_t) address_space->top_page_table,
        HHDM_CAST(uint64_t, boot_info),
        ((uint16_t) MAJOR_VERSION << 8) | MINOR_VERSION
    );
    __builtin_unreachable();
}
