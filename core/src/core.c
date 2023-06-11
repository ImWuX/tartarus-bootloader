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
#ifdef __AMD64
#include <drivers/lapic.h>
#endif
#include <smp.h>

#if defined __BIOS && defined __AMD64
static void testap() {
    uint8_t value = 'X';
    uint16_t port = 0x3F8;
    asm volatile("outb %0, %1" : : "a" (value), "Nd" (port));
    while(true) asm volatile("hlt");
}
#endif

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
#ifdef __AMD64
    void *smp_rsv_page = pmm_alloc_pages(1, PMM_AREA_CONVENTIONAL); // Allocate a page early to get one lower than 0x100000
    if((uintptr_t) smp_rsv_page >= 0x100000) log_panic("CORE", "Unable to reserve a page for SMP");
#endif
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
    if(!rsdp) log_panic("CORE", "Could not locate RSDP");

#ifdef __AMD64
    void *pml4 = vmm_initialize();
    if(!lapic_supported()) log_panic("CORE", "Local APIC not supported");
    acpi_sdt_header_t *madt = acpi_find_table(rsdp, "APIC");
    if(!madt) log_panic("CORE", "No MADT table present");
    uint64_t *woa = smp_initialize_aps(madt, (uintptr_t) smp_rsv_page, pml4);
#endif

    // SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    while(true);
}