#include "fb.h"
#include <graphics/basicfont.h>
#include <log.h>
#include <libc.h>
#include <core.h>
#if defined __AMD64 && defined __BIOS
#include <graphics/vesa.h>
#include <int.h>
#endif
#ifdef __UEFI
#include <efi.h>
#endif

#if defined __AMD64 && defined __BIOS
#define MEMORY_MODEL_RGB 6
#define LINEAR_FRAMEBUFFER (1 << 7)
#define USE_LFB (1 << 14)

inline static bool get_mode_info(uint16_t mode, vesa_vbe_mode_info_t *dest) {
    int_regs_t regs = {
        .eax = 0x4F01,
        .ecx = mode,
        .es = int_16bit_segment(dest),
        .edi = int_16bit_offset(dest)
    };
    int_exec(0x10, &regs);
    return (regs.eax & 0xFFFF) != 0x4F;
}

bool fb_acquire(uint32_t target_width, uint32_t target_height, fb_t *out) {
    vesa_vbe_info_t vbe_info = {};
    ((uint8_t *) &vbe_info)[0] = (uint8_t) 'V';
    ((uint8_t *) &vbe_info)[1] = (uint8_t) 'B';
    ((uint8_t *) &vbe_info)[2] = (uint8_t) 'E';
    ((uint8_t *) &vbe_info)[3] = (uint8_t) '2';

    int_regs_t regs = {
        .eax = 0x4F00,
        .es = int_16bit_segment(&vbe_info),
        .edi = int_16bit_offset(&vbe_info)
    };
    int_exec(0x10, &regs);
    if((regs.eax & 0xFFFF) != 0x4F) return true;
    bool closest_found = false;
    uint16_t closest = 0;
    uint64_t closest_diff = UINT64_MAX;

    uint16_t *modes = (uint16_t *) int_16bit_desegment(vbe_info.video_modes_segment, vbe_info.video_modes_offset);
    vesa_vbe_mode_info_t mode_info;
    while(*modes != 0xFFFF) {
        uint16_t mode = *modes++;
        if(get_mode_info(mode, &mode_info)) continue;
        if(mode_info.memory_model != MEMORY_MODEL_RGB) continue;
        if(mode_info.red_mask != 8 || mode_info.blue_mask != 8 || mode_info.green_mask != 8 || mode_info.reserved_mask != 8) continue;
        if(mode_info.red_position != 16 || mode_info.green_position != 8 || mode_info.blue_position != 0 || mode_info.reserved_position != 24) continue;
        if(!(mode_info.attributes & LINEAR_FRAMEBUFFER)) continue;
        if(mode_info.bpp != 32) continue;

        int64_t signed_diff = ((int64_t) target_width - (int64_t) mode_info.width) + ((int64_t) target_height - (int64_t) mode_info.height);
        uint64_t diff = signed_diff < 0 ? -signed_diff : signed_diff;
        if(diff >= closest_diff) continue;
        closest_found = true;
        closest_diff = diff;
        closest = mode;
    }
    if(!closest_found) return true;

    memset(&regs, 0, sizeof(int_regs_t));
    regs.eax = 0x4F02;
    regs.ebx = closest | USE_LFB;
    int_exec(0x10, &regs);
    if((regs.eax & 0xFFFF) != 0x4F) return true;

    get_mode_info(closest, &mode_info);
    out->address = mode_info.framebuffer;
    if(vbe_info.version_major < 3) {
        out->pitch = mode_info.pitch;
    } else {
        out->pitch = mode_info.linear_pitch;
    }
    out->pitch /= mode_info.bpp / 8;
    out->width = mode_info.width;
    out->height = mode_info.height;
    out->size = out->height * out->pitch * (mode_info.bpp / 8);
    return false;
}
#elif defined __UEFI
bool fb_acquire(uint32_t target_width, uint32_t target_height, fb_t *out) {
    bool closest_found = false;
    UINTN closest = 0;
    UINT32 closest_difference = UINT32_MAX;
    for(UINT32 i = 0; i < g_gop->Mode->MaxMode; i++) {
        UINTN info_size;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        EFI_STATUS status = g_gop->QueryMode(g_gop, i, &info_size, &info);
        if(EFI_ERROR(status)) continue;
        UINT32 difference = info->HorizontalResolution > target_width ? info->HorizontalResolution - target_width : target_width - info->HorizontalResolution;
        difference += info->VerticalResolution > target_height ? info->VerticalResolution - target_height : target_height - info->VerticalResolution;
        if(difference >= closest_difference) continue;
        closest_found = true;
        closest_difference = difference;
        closest = i;
    }
    if(!closest_found) return true;
    EFI_STATUS status = g_gop->SetMode(g_gop, closest);
    if(EFI_ERROR(status)) return true;
    out->address = g_gop->Mode->FrameBufferBase;
    out->size = g_gop->Mode->FrameBufferSize;
    out->width = g_gop->Mode->Info->HorizontalResolution;
    out->height = g_gop->Mode->Info->VerticalResolution;
    out->pitch = g_gop->Mode->Info->PixelsPerScanLine;
    return false;
}
#else
#error Invalid target or missing implementation
#endif

void fb_char(fb_t *fb, int cx, int cy, char c, uint32_t color) {
    uint8_t *font_char = &g_basicfont[c * BASICFONT_HEIGHT];
    uint64_t offset = cx + cy * fb->pitch;
    for(uint32_t y = 0; y < BASICFONT_HEIGHT && cy + y < fb->height; y++) {
        for(uint32_t x = 0; x < BASICFONT_WIDTH && cx + x < fb->width; x++) {
            if(font_char[y] & (1 << (BASICFONT_WIDTH - x))) {
                ((uint32_t *) fb->address)[offset + x] = color;
            }
        }
        offset += fb->pitch;
    }
}

void fb_clear(fb_t *fb, uint32_t color) {
    uint64_t offset = 0;
    for(uint32_t y = 0; y < fb->height; y++) {
        for(uint32_t x = 0; x < fb->width; x++) {
            ((uint32_t *) fb->address)[offset + x] = color;
        }
        offset += fb->pitch;
    }
}