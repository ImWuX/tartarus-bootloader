#ifndef GRAPHICS_GOP_H
#define GRAPHICS_GOP_H

#include <efi.h>

EFI_STATUS gop_initialize(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, UINTN target_width, UINTN target_height);

#endif