#include "core.h"
#include <tartarus.h>
#include <stdint.h>
#include <log.h>
#include <graphics/fb.h>
#include <memory/pmm.h>
#include <memory/heap.h>
#include <disk.h>

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
    if(EFI_ERROR(status)) log_panic("Unable to locate GOP protocol");
    status = fb_initialize(gop, 1920, 1080);
    if(EFI_ERROR(status)) log_panic("Unable to initialize a GOP");

    pmm_initialize();
    disk_initialize();

    disk_t *disk = g_disks;
    while(disk) {
        log(">> %i, present: %i, isPart: %i, lowLba: %x, lastBlock: %x\n",
            (uint64_t) disk->io->Media->MediaId,
            (uint64_t) disk->io->Media->MediaPresent,
            (uint64_t) disk->io->Media->LogicalPartition,
            (uint64_t) disk->io->Media->LowestAlignedLba,
            (uint64_t) disk->io->Media->LastBlock
        );
        disk = disk->next;
    }

    // SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    while(true);
}
#endif

#if defined __BIOS && defined __AMD64
[[noreturn]] void core() {
    fb_initialize(1920, 1080);
    pmm_initialize();
    disk_initialize();

    while(true);
}
#endif
