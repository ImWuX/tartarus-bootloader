#include <log.h>
#include <e820.h>
#include <vmm.h>
#include <pmm.h>
#include <disk.h>
#include <fat32.h>
#include <elf.h>
#include <vesa.h>

#define KERNEL_FILE "KERNEL  SYS"

typedef struct {
    uint64_t entry;
    uint64_t stack_address;
    uint8_t boot_drive;
    uint64_t memory_map;
    uint16_t memory_map_length;
    uint64_t framebuffer;
    uint64_t hhdm_address;
} __attribute__((packed)) tartarus_internal_params_t;

tartarus_internal_params_t *load() {
    log_clear();
    log("Tartarus | Protected Mode\n");

    disk_initialize();

    e820_load();
    pmm_initialize();
    log("Tartarus | Physical Memory Initialized\n");

    vmm_initialize(g_memap, g_memap_length);
    log("Tartarus | Virtual Memory initialized\n");

    tartarus_internal_params_t *params = pmm_request_page();
    params->boot_drive = disk_drive();
    params->memory_map = (uint32_t) g_memap;
    params->hhdm_address = HHDM_OFFSET;

    void *framebuffer = vesa_setup(1920, 1080, 32);
    params->framebuffer = (uint32_t) framebuffer;
    log("Tartarus | Vesa Initialized\n");

    fat32_initialize();
    log("Tartarus | Fat32 Initialized\n");

    uint32_t kernel_cluster = fat32_root_find((uint8_t *) KERNEL_FILE);
    log("Tartarus | Found kernel\n");

    elf64_addr_t entry = elf_read_file(kernel_cluster);
    if(entry == 0) {
        log_panic("Failed to load ELF kernel file");
        __builtin_unreachable();
    }
    params->entry = entry;
    log("Tartarus | Kernel Loaded");

    params->stack_address = (uint64_t) (uint32_t) pmm_request_linear_pages(16) + 16 * 0x1000;

    params->memory_map_length = g_memap_length;

    return params;
}