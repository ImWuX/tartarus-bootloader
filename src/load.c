#include <log.h>
#include <e820.h>
#include <vmm.h>
#include <pmm.h>
#include <disk.h>
#include <fat32.h>
#include <elf.h>

#define KERNEL_FILE "KERNEL  SYS"

void load() {
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

    char *kernel_name = KERNEL_FILE;
    dir_entry_t *directory = read_root_directory();
    while(directory != 0) {
        log(":: ");
        for(uint8_t i = 0; i < 11; i++) {
            log_putchar(directory->fd.name[i]);
        }
        log("\n");

        bool is_kernel = true;
        for(int i = 0; i < 11; i++) {
            if(directory->fd.name[i] != (uint8_t) kernel_name[i]) {
                is_kernel = false;
                break;
            }
        }
        if(is_kernel) {
            elf64_addr_t entry = elf_read_file(&directory->fd);
            if(entry == 0) log_panic("Kernel failed to load");
        }
        directory = directory->last_entry;
    }
}