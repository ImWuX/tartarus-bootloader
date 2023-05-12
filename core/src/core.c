#include "core.h"
#include <tartarus.h>
#include <stdint.h>
#if defined __BIOS && defined __AMD64
#include <e820.h>
#endif
#ifdef __UEFI
#include <graphics/gop.h>
#endif

#ifdef __UEFI
EFI_SYSTEM_TABLE *g_st;

[[noreturn]] void temp_panic(EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"PANIC\n");
    asm volatile("cli\nhlt");
    __builtin_unreachable();
}

[[noreturn]] EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    g_st = SystemTable;

    EFI_STATUS status;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    status = SystemTable->BootServices->LocateProtocol(&gop_guid, NULL, (void **) &gop);
    if(EFI_ERROR(status)) temp_panic(SystemTable);
    status = gop_initialize(gop, 1920, 1080);
    if(EFI_ERROR(status)) temp_panic(SystemTable);

    void *buffer;
    g_st->BootServices->AllocatePool(EfiBootServicesData, 30 * 30 * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL), &buffer);
    for(UINTN x = 0; x < 30; x++) {
        for(UINTN y = 0; y < 30; y++) {
            ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) buffer)[y * 30 + x].Reserved = 0;
            ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) buffer)[y * 30 + x].Red = 0;
            ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) buffer)[y * 30 + x].Green = 0;
            ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) buffer)[y * 30 + x].Blue = 0xFF;
        }
    }

    gop->Blt(gop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) buffer, EfiBltBufferToVideo, 0, 0, 100, 100, 30, 30, 0);


    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    status = SystemTable->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
    if(status == EFI_BUFFER_TOO_SMALL) {
        status = SystemTable->BootServices->AllocatePool(EfiBootServicesData, map_size, (void **) &map);
        if(EFI_ERROR(status)) temp_panic(SystemTable);
        status = SystemTable->BootServices->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &descriptor_version);
        if(EFI_ERROR(status)) temp_panic(SystemTable);
    }

    // for(UINTN i = 0; i < map_size; i += descriptor_size) {
    //     SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[MMENTRY]: ");
    //     SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\n\r");
    // }

    SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);

    while(true);
}
#else
[[noreturn]] void core() {
#if defined __BIOS && defined __AMD64
    e820_load();
#endif
    while(true);
}
#endif
