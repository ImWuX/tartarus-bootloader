#include <stdint.h>
#include <cpuid.h>
#include <common/log.h>
#include <common/config.h>
#include <common/elf.h>
#include <common/fb.h>
#include <lib/str.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <memory/heap.h>
#include <sys/msr.x86_64.h>
#include <sys/e820.x86_64.bios.h>
#include <sys/lapic.x86_64.h>
#include <sys/smp.x86_64.h>
#include <dev/disk.h>
#include <dev/acpi.h>
#include <fs/vfs.h>
#include <fs/fat.h>
#include <protocol/protocol.h>

#define CPUID_NX (1 << 20)
#define MSR_EFER 0xC0000080
#define MSR_EFER_NX (1 << 11)
#define E820_MAX 512

#define HHDM_MIN_SIZE 0x100000000
#define HHDM_OFFSET 0xFFFF800000000000
#define HHDM_FLAGS (VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC)

typedef int SYMBOL[];

extern SYMBOL __tartarus_start;
extern SYMBOL __tartarus_end;

bool g_cpu_nx_support = false;

static void log_sink(char c) {
    asm volatile("outb %0, %1" : : "a" (c), "Nd" (0x3F8));
}

[[noreturn]] void core() {
    log_sink('\n');
    log_init(log_sink);
    log("CORE", "Tartarus Bootloader (BIOS)");

    // Enable NX
    unsigned int edx = 0, unused;
    if(__get_cpuid(0x80000001, &unused, &unused, &unused, &edx) != 0) g_cpu_nx_support = (edx & CPUID_NX) != 0;
    if(g_cpu_nx_support) {
        msr_write(MSR_EFER, msr_read(MSR_EFER) | MSR_EFER_NX);
    } else {
        log_warning("CORE", "CPU does not support EFER.NXE");
    }

    // Setup physical memory
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
    pmm_init_sanitize();
    log("CORE", "Initialized physical memory (%i memory map entries)", e820_size);

    // Protect initial stack & tartarus
    // TODO: the stack claim if flimsy
    if(pmm_convert(TARTARUS_MEMORY_MAP_TYPE_USABLE, TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE, 0x1000, 0x7000)) log_panic("CORE", "Failed to claim stack memory");
    if(pmm_convert(TARTARUS_MEMORY_MAP_TYPE_USABLE, TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE, (uintptr_t) __tartarus_start, (uintptr_t) __tartarus_end - (uintptr_t) __tartarus_start)) log_panic("CORE", "Failed to claim kernel memory");

    // Allocate a page early to get one low enough for smp startup
    void *smp_reserved_page = pmm_alloc_page(PMM_AREA_CONVENTIONAL);
    if((uintptr_t) smp_reserved_page >= 0x100000) {
        pmm_free_page(smp_reserved_page);
        smp_reserved_page = NULL;
    }

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
    log("CORE", "Config loaded");

    // Initialize ACPI
    acpi_rsdp_t *rsdp = acpi_find_rsdp();
    if(!rsdp) log_panic("CORE", "Could not locate RSDP");

    // Find kernel
    char *kernel_path = config_read_string(config, "KERNEL");
    if(kernel_path == NULL) log_panic("CORE", "No kernel path provided in config");
    vfs_node_t *kernel_node = vfs_lookup(config_node->vfs, kernel_path);
    if(kernel_node == NULL) log_panic("CORE", "Kernel not present at \"%s\"", kernel_path);

    // FB
    int scrw = config_read_int(config, "SCRW", 1920);
    int scrh = config_read_int(config, "SCRH", 1080);
    log("CORE", "Screen size is presumed to be %ix%i", scrw, scrh);

    fb_t fb;
    if(fb_acquire(scrw, scrh, false, &fb)) log_panic("CORE", "Failed to retrieve framebuffer");
    log("CORE", "Acquired framebuffer (%ux%u)", fb.width, fb.height);

    // Protocol
    char *protocol = config_read_string(config, "PROTOCOL");
    if(protocol == NULL) log_panic("CORE", "No protocol defined in config");
    log("CORE", "Protocol is %s", protocol);

    // Protocol: Linux
    if(strcmp(protocol, "linux") == 0) {
        char *cmdline = config_read_string(config, "CMD");

        char *ramdisk_path = config_read_string(config, "INITRD");
        if(ramdisk_path == NULL) log_panic("CORE", "No initrd path provided in config");
        vfs_node_t *ramdisk_node = vfs_lookup(config_node->vfs, ramdisk_path);
        if(ramdisk_node == NULL) log_panic("CORE", "Initrd was not present at \"%s\"", ramdisk_path);

        config_free(config);
        protocol_linux(kernel_node, ramdisk_node, cmdline, rsdp, e820, e820_size, fb);
    }

    // Protocol: Tartarus
    if(strcmp(protocol, "tartarus") == 0) {
        // Setup address space
        void *address_space = vmm_create_address_space();
        vmm_map(address_space, PMM_PAGE_SIZE, PMM_PAGE_SIZE, HHDM_MIN_SIZE - PMM_PAGE_SIZE, HHDM_FLAGS);
        vmm_map(address_space, PMM_PAGE_SIZE, HHDM_OFFSET + PMM_PAGE_SIZE, HHDM_MIN_SIZE - PMM_PAGE_SIZE, HHDM_FLAGS);

        uint64_t hhdm_size = HHDM_MIN_SIZE;
        for(uint16_t i = 0; i < g_pmm_map_size; i++) {
            if(g_pmm_map[i].base + g_pmm_map[i].length < HHDM_MIN_SIZE) continue;
            uint64_t base = g_pmm_map[i].base;
            uint64_t length = g_pmm_map[i].length;
            if(base < HHDM_MIN_SIZE) {
                length -= HHDM_MIN_SIZE - base;
                base = HHDM_MIN_SIZE;
            }
            if(base % PMM_PAGE_SIZE != 0) {
                length += base % PMM_PAGE_SIZE;
                base -= base % PMM_PAGE_SIZE;
            }
            if(length % PMM_PAGE_SIZE != 0) length += PMM_PAGE_SIZE - length % PMM_PAGE_SIZE;
            if(base + length > hhdm_size) hhdm_size = base + length;
            vmm_map(address_space, base, base, length, HHDM_FLAGS);
            vmm_map(address_space, base, HHDM_OFFSET + base, length, HHDM_FLAGS);
        }
        log("CORE", "Initialized virtual memory");

        // Initialize SMP
        if(config_read_bool(config, "SMP", true)) {
            if(smp_reserved_page == NULL) log_panic("CORE", "Unable to initialize SMP. No reserved SMP init page");
            if(!lapic_is_supported()) log_panic("CORE", "LAPIC not supported. LAPIC is required for SMP initialization");
            acpi_sdt_header_t *madt = acpi_find_table(rsdp, "APIC");
            if(!madt) log_panic("CORE", "ACPI MADT table not present");
            smp_cpu_t *cpus = smp_initialize_aps(madt, smp_reserved_page, address_space);
            log("CORE", "Initialized SMP");
        }

        config_free(config);
        log_panic("CORE", "tartarus protocol temporarily unimplemented");
    }
    config_free(config);
    log_panic("CORE", "Invalid protocol %s", protocol);
    __builtin_unreachable();
}