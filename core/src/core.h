#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#ifdef __UEFI
#include <efi.h>
#endif

#ifdef __UEFI
extern EFI_SYSTEM_TABLE *g_st;
extern EFI_HANDLE g_imagehandle;
extern EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop;
#endif

#endif