#include "gop.h"

EFI_STATUS gop_initialize(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, UINTN target_width, UINTN target_height) {
    for(UINT32 i = 0; i < gop->Mode->MaxMode; i++) {
        UINTN info_size;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        EFI_STATUS status = gop->QueryMode(gop, i, &info_size, &info);
        if(EFI_ERROR(status)) continue;
        if(info->HorizontalResolution != target_width || info->VerticalResolution != target_height) continue;
        gop->SetMode(gop, i);
        break;
    }

    return EFI_SUCCESS;
}