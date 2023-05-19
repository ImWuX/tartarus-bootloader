#ifndef GRAPHICS_FB_H
#define GRAPHICS_FB_H

#include <stdint.h>
#ifdef __UEFI
#include <efi.h>
#endif

extern bool g_fb_initialized;
extern int g_fb_width;
extern int g_fb_height;

void fb_char(int x, int y, char c, uint32_t color);
void fb_clear(uint32_t color);

#ifdef __UEFI
EFI_STATUS fb_initialize(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, UINTN target_width, UINTN target_height);
#endif

#if defined __AMD64 && defined __BIOS
void fb_initialize(uint16_t target_width, uint16_t target_height);
#endif

#endif