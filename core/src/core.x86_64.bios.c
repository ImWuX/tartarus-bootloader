#include <stdint.h>
#include <cpuid.h>
#include <log.h>
#include <config.h>
#include <memory/pmm.h>
#include <memory/heap.h>
#include <sys/msr.x86_64.h>
#include <sys/e820.x86_64.bios.h>
#include <dev/disk.h>
#include <fs/vfs.h>
#include <fs/fat.h>

#define CPUID_NX (1 << 20)
#define MSR_EFER 0xC0000080
#define MSR_EFER_NX (1 << 11)
#define E820_MAX 512

typedef int SYMBOL[];

extern SYMBOL __tartarus_start;
extern SYMBOL __tartarus_end;

static bool g_nx_support = false;

static void log_sink(char c) {
    asm volatile("outb %0, %1" : : "a" (c), "Nd" (0x3F8));
}

static int parse_e820() {
    e820_entry_t e820[E820_MAX];
    int e820_size = e820_load((void *) &e820, E820_MAX);
    for(int i = 0; i < e820_size; i++) {
        tartarus_memory_map_type_t type;
        switch(e820[i].type) {
            case E820_TYPE_USABLE: type = TARTARUS_MEMORY_MAP_TYPE_USABLE; break;
            case E820_TYPE_ACPI_RECLAIMABLE: type = TARTARUS_MEMORY_MAP_TYPE_ACPI_RECLAIMABLE; break;
            case E820_TYPE_ACPI_NVS: type = TARTARUS_MEMORY_MAP_TYPE_ACPI_NVS; break;
            case E820_TYPE_BAD: type = TARTARUS_MEMORY_MAP_TYPE_BAD; break;
            case E820_TYPE_RESERVED:
            default: type = TARTARUS_MEMORY_MAP_TYPE_RESERVED; break;
        }
        pmm_init_add((tartarus_memory_map_entry_t) { .base = e820[i].address, .length = e820[i].length, .type = type });
    }
    return e820_size;
}

[[noreturn]] void core() {
    log_sink('\n');
    log_init(log_sink);
    log("CORE", "Tartarus Bootloader (BIOS)");

    // Enable NX
    unsigned int edx = 0, unused;
    if(__get_cpuid(0x80000001, &unused, &unused, &unused, &edx) != 0) g_nx_support = (edx & CPUID_NX) != 0;
    if(g_nx_support) {
        msr_write(MSR_EFER, msr_read(MSR_EFER) | MSR_EFER_NX);
    } else {
        log_warning("CORE", "CPU does not support EFER.NXE");
    }

    // Setup physical memory
    int e820_size = parse_e820();
    pmm_init_sanitize();
    log("CORE", "Initialized physical memory (%i memory map entries)", e820_size);

    // Protect initial stack
    pmm_convert(TARTARUS_MEMORY_MAP_TYPE_USABLE, TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE, 0x3000, 0x5000);
    pmm_convert(TARTARUS_MEMORY_MAP_TYPE_USABLE, TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE, (uintptr_t) __tartarus_start, (uintptr_t) __tartarus_end - (uintptr_t) __tartarus_start);

    // Allocate a page early to get one low enough for smp startup
    void *smp_rsv_page = pmm_alloc_page(PMM_AREA_CONVENTIONAL);
    if((uintptr_t) smp_rsv_page >= 0x100000) log_panic("CORE", "Unable to reserve a low page for SMP startup");

    // Initialize disk
    disk_initialize();
    int disk_count = 0;
    for(disk_t *disk = g_disks; disk != NULL; disk = disk->next) disk_count++;
    log("CORE", "Initialized disks (%i disks)", disk_count);

    // Locate config
    vfs_node_t *config_node = NULL;
    for(disk_t *disk = g_disks; disk != NULL; disk = disk->next) {
        for(disk_part_t *partition = disk->partitions; partition != NULL; partition = partition->next) {
            vfs_t *fat_fs = fat_initialize(partition);
            if(fat_fs == NULL) continue;
            vfs_node_t *node = vfs_lookup(fat_fs, "/tartarus.cfg");
            if(node == NULL) continue;
            config_node = node;
        }
    }
    if(config_node == NULL) log_panic("CORE", "Could not locate a config file");
    config_t *config = config_parse(config_node);
    log("CORE", "Loaded config");


    config_free(config);
    for(;;);
}