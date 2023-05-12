#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#ifdef __UEFI
#include <efi.h>

extern EFI_SYSTEM_TABLE *g_st;
#endif

void core();

#endif