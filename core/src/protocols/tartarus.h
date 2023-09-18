#ifndef PROTOCOLS_TARTARUS_H
#define PROTOCOLS_TARTARUS_H

#include <tartarus.h>
#include <memory/vmm.h>
#include <drivers/acpi.h>
#include <graphics/fb.h>
#include <module.h>
#include <smp.h>
#include <elf.h>

[[noreturn]] void protocol_tartarus_handoff(
    uint64_t boot_info_offset,
    elf_loaded_image_t *kernel,
    acpi_rsdp_t *rsdp,
    vmm_address_space_t address_space,
    fb_t *framebuffer,
    tartarus_mmap_entry_t *map,
    uint16_t map_size,
    uint64_t hhdm_base,
    uint64_t hhdm_size,
    module_t *modules,
    smp_cpu_t *cpus
);

#endif