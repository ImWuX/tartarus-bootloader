#ifndef PROTOCOLS_TARTARUS_H
#define PROTOCOLS_TARTARUS_H

#include <tartarus.h>
#include <memory/vmm.h>
#include <drivers/acpi.h>
#include <graphics/fb.h>

[[noreturn]] void protocol_tartarus_handoff(
    tartarus_elf_image_t *kernel,
    acpi_rsdp_t *rsdp,
    vmm_address_space_t address_space,
    fb_t *framebuffer,
    tartarus_mmap_entry_t *map,
    int map_size
);

#endif