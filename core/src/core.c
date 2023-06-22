#include "core.h"
#include <tartarus.h>
#include <stdint.h>
#include <log.h>
#include <libc.h>
#include <graphics/fb.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <memory/heap.h>
#include <drivers/disk.h>
#include <drivers/acpi.h>
#include <fs/fat.h>
#include <elf.h>
#include <config.h>
#include <smp.h>
#include <protocols/tartarus.h>
#ifdef __AMD64
#include <drivers/lapic.h>
#endif

#ifdef __UEFI
EFI_SYSTEM_TABLE *g_st;
EFI_HANDLE g_imagehandle;
EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop;

[[noreturn]] EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    g_st = SystemTable;
    g_imagehandle = ImageHandle;

    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    status = SystemTable->BootServices->LocateProtocol(&gop_guid, NULL, (void **) &gop);
    if(EFI_ERROR(status)) log_panic("CORE", "Unable to locate GOP protocol");
    g_gop = gop;
#elif defined __BIOS && defined __AMD64
typedef int SYMBOL[];

extern SYMBOL __tartarus_start;
extern SYMBOL __tartarus_end;

[[noreturn]] void core() {
#else
#error Invalid target
#endif
    fb_t initial_fb;
    if(fb_aquire(1920, 1080, &initial_fb)) log_panic("CORE", "Failed to aquire the initial framebuffer");
    log_set_fb(&initial_fb);
    pmm_initialize();
#ifdef __AMD64
#ifdef __BIOS
    pmm_convert(TARTARUS_MEMAP_TYPE_USABLE, TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE, 0x6000, 0x1000); // Protect initial stack
    pmm_convert(TARTARUS_MEMAP_TYPE_USABLE, TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE, (uintptr_t) __tartarus_start, (uintptr_t) __tartarus_end - (uintptr_t) __tartarus_start);
#endif
    void *smp_rsv_page = pmm_alloc_page(PMM_AREA_CONVENTIONAL); // Allocate a page early to get one lower than 0x100000
    if((uintptr_t) smp_rsv_page >= 0x100000) log_panic("CORE", "Unable to reserve a page for SMP");
#endif
    disk_initialize();

    fat_file_t *cfg;
    disk_t *disk = g_disks;
    while(disk) {
        disk_part_t *partition = disk->partitions;
        while(partition) {
            fat_info_t *fat_info = fat_initialize(partition);
            if(fat_info) {
                cfg = config_find(fat_info);
                if(cfg) break;
                heap_free(fat_info);
            }
            partition = partition->next;
        }
        if(cfg) break;
        disk = disk->next;
    }
    if(!cfg) log_panic("CORE", "Could not locate a config file");

    acpi_rsdp_t *rsdp = acpi_find_rsdp();
    if(!rsdp) log_panic("CORE", "Could not locate RSDP");

    vmm_address_space_t address_space = vmm_initialize();
#ifdef __AMD64
    if(!lapic_supported()) log_panic("CORE", "Local APIC not supported");
    acpi_sdt_header_t *madt = acpi_find_table(rsdp, "APIC");
    if(!madt) log_panic("CORE", "No MADT table present");
    uint64_t *woa = smp_initialize_aps(madt, (uintptr_t) smp_rsv_page, address_space);
#endif

    char *kernel_name;
    if(config_get_string_ext(cfg, "KERNEL", &kernel_name)) log_panic("CORE", "No kernel file specified");
    fat_file_t *kernel = fat_root_lookup(cfg->info, kernel_name);
    if(!kernel) log_panic("CORE", "Could not find the kernel (%s)\n", kernel_name);

    tartarus_elf_image_t *kernel_image = elf_load(kernel, address_space);
    if(!kernel_image) log_panic("CORE", "Failed to load kernel\n");
    if(!kernel_image->entry) log_panic("CORE", "Kernel has no entry point\n");
    log("Kernel Loaded\n");

    char *protocol;
    if(config_get_string_ext(cfg, "PROTOCOL", &protocol)) log_panic("CORE", "No protocol specified");
    if(strcmp(protocol, "TARTARUS") == 0) protocol_tartarus_handoff(kernel_image, rsdp, address_space, &initial_fb);
    log_panic("CORE", "Invalid protocol %s\n", protocol);
}