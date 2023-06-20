#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#ifdef __UEFI
#include <efi.h>
#endif

#ifdef __UEFI
extern EFI_SYSTEM_TABLE *g_st;
extern EFI_HANDLE g_imagehandle;
#endif

#if defined __BIOS && defined __AMD64
void core();
#endif

#endif