#include <log.h>
#include <e820.h>
#include <vmm.h>
#include <pmm.h>
#include <disk.h>
#include <fat32.h>
#include <elf.h>
#include <vesa.h>
#include <config.h>
#include <acpi.h>

typedef struct {
    uint64_t entry;
    uint64_t stack_address;
    uint8_t boot_drive;
    uint64_t kernel_symbols;
    uint64_t kernel_symbols_size;
    uint64_t memory_map;
    uint16_t memory_map_length;
    uint64_t framebuffer;
    uint64_t hhdm_start;
    uint64_t hhdm_end;
    uint64_t rsdp;
} __attribute__((packed)) tartarus_internal_params_t;

tartarus_internal_params_t *load() {
    log_clear();
    log("Tartarus | Protected Mode\n");

    disk_initialize();
    log("Tartarus | Disk Initialized\n");

    e820_load();
    log("Tartarus | E820 Map Loaded\n");

    pmm_initialize();
    log("Tartarus | Physical Memory Initialized\n");

    uint64_t highest_mapped_address;
    vmm_initialize(g_memap, g_memap_length, &highest_mapped_address);
    log("Tartarus | Virtual Memory Initialized\n");

    fat32_initialize();
    log("Tartarus | Fat32 Initialized\n");

    config_initialize();
    log("Tartarus | Config Initialized\n");

    tartarus_internal_params_t *params = pmm_request_page();
    params->boot_drive = disk_drive();
    params->memory_map = (uint32_t) g_memap;
    params->hhdm_start = HHDM_OFFSET;
    params->hhdm_end = HHDM_OFFSET + highest_mapped_address;
    if(config_read_bool("acpi_find_rsdp", false)) {
        params->rsdp = (uint64_t) acpi_find_rsdp();
        log("Tartarus | RSDP At: $\n", params->rsdp);
    }

    if(config_read_bool("vesa_setup", true)) {
        void *framebuffer = vesa_setup(config_read_int("vesa_target_width", 1920), config_read_int("vesa_target_height", 1080), 32);
        params->framebuffer = (uint32_t) framebuffer;
        log("Tartarus | Vesa Initialized\n");
    }

    fat_file_info kernel_file_info;
    if(fat32_root_find((uint8_t *) config_read_string("kernel_filename", "KERN    SYS"), &kernel_file_info)) log_panic("Could not find the kernel file");
    log("Tartarus | Found Kernel\n");

    char *symbols = config_read_string("kernel_symbols", "");
    if(*symbols) {
        fat_file_info symbols_file_info;
        if(fat32_root_find((uint8_t *) symbols, &symbols_file_info)) log_panic("Could not find the kernel symbols file");
        uint32_t pages = (symbols_file_info.size + 0xFFF) / 0x1000;
        void *addr = pmm_request_linear_pages(pages);
        log("ksize $, $\n", (uint64_t) symbols_file_info.size, (uint64_t) pages);
        if(fat32_read(symbols_file_info.cluster_number, 0, symbols_file_info.size, addr)) log_panic("Failed to load kernel file");
        params->kernel_symbols = (uint32_t) addr;
        params->kernel_symbols_size = symbols_file_info.size;
    }

    elf64_addr_t entry = elf_read_file(kernel_file_info.cluster_number);
    if(entry == 0) {
        log_panic("Failed to load kernel");
        __builtin_unreachable();
    }
    params->entry = entry;
    log("Tartarus | Kernel Loaded");

    params->stack_address = (uint32_t) pmm_request_linear_pages(16) + 16 * 0x1000;
    params->memory_map_length = g_memap_length;

    return params;
}