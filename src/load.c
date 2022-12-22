#include <log.h>
#include <e820.h>
#include <vmm.h>
#include <pmm.h>
#include <disk.h>
#include <fat32.h>
#include <elf.h>

#define KERNEL_FILE "KERNEL  SYS"

uint64_t load() {
    log_clear();
    log("Tartarus | Protected Mode\n");

    disk_initialize();

    e820_load();
    pmm_initialize();
    log("Tartarus | Physical Memory Initialized\n");

    vmm_initialize(g_memap, g_memap_length);
    log("Tartarus | Virtual Memory initialized\n");

    fat32_initialize();
    log("Tartarus | Fat32 Initialized\n");

    uint32_t kernel_cluster = fat32_root_find((uint8_t *) KERNEL_FILE);
    log("Tartarus | Found kernel\n");

    elf64_addr_t entry = elf_read_file(kernel_cluster);
    if(entry == 0) {
        log_panic("Failed to load ELF kernel file");
        __builtin_unreachable();
    }
    log("Tartarus | Kernel Loaded");

    return entry;
}