#include "core.h"
#include <tartarus.h>
#include <stdint.h>
#include <log.h>
#include <graphics/fb.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <memory/heap.h>
#include <drivers/disk.h>
#include <drivers/acpi.h>

#ifdef __UEFI
EFI_SYSTEM_TABLE *g_st;
EFI_HANDLE g_imagehandle;

[[noreturn]] EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    g_st = SystemTable;
    g_imagehandle = ImageHandle;

    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    status = SystemTable->BootServices->LocateProtocol(&gop_guid, NULL, (void **) &gop);
    if(EFI_ERROR(status)) log_panic("CORE", "Unable to locate GOP protocol");
    status = fb_initialize(gop, 1920, 1080);
    if(EFI_ERROR(status)) log_panic("CORE", "Unable to initialize a GOP");
#endif

#if defined __BIOS && defined __AMD64
[[noreturn]] void core() {
    fb_initialize(1920, 1080);
#endif
    pmm_initialize();
    disk_initialize();

    disk_t *disk = g_disks;
    while(disk) {
        log(">> Disk %i { SectorCount: %x, SectorSize: %i, Writable: %i }\n",
            (uint64_t) disk->id,
            (uint64_t) disk->sector_count,
            (uint64_t) disk->sector_size,
            (uint64_t) disk->writable
        );
        disk_part_t *partition = disk->partitions;
        while(partition) {
            log("    >> Partition { LBA: %x, Size: %x }\n", partition->lba, partition->size);
            partition = partition->next;
        }
        disk = disk->next;
    }

    acpi_rsdp_t *rsdp = acpi_find_rsdp();
    log(">> RSDP: %x\n", (uint64_t) rsdp);
    acpi_sdt_header_t *madt = acpi_find_table(rsdp, "APIC");
    log(">> MADT: %x\n", (uint64_t) madt);

    vmm_initialize();

    // SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    while(true);
}