#include "fb.h"
#include <graphics/basicfont.h>
#include <log.h>
#include <libc.h>
#if defined __AMD64 && defined __BIOS
#include <graphics/vesa.h>
#include <int.h>
#endif

bool g_fb_initialized = false;
int g_fb_width;
int g_fb_height;

#ifdef __UEFI
EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop;

#define FB_BASE g_gop->Mode->FrameBufferBase
#define PITCH g_gop->Mode->Info->PixelsPerScanLine

EFI_STATUS fb_initialize(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, UINTN target_width, UINTN target_height) {
    g_gop = gop;
    for(UINT32 i = 0; i < gop->Mode->MaxMode; i++) {
        UINTN info_size;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        EFI_STATUS status = gop->QueryMode(gop, i, &info_size, &info);
        if(EFI_ERROR(status)) continue;
        if(info->HorizontalResolution != target_width || info->VerticalResolution != target_height) continue;
        gop->SetMode(gop, i);
        g_fb_width = g_gop->Mode->Info->HorizontalResolution;
        g_fb_height = g_gop->Mode->Info->VerticalResolution;
        g_fb_initialized = true;
        return EFI_SUCCESS;
    }
    return EFI_UNSUPPORTED;
}
#endif

#if defined __BIOS && defined __AMD64

static uint32_t g_fb_base;
static uint16_t g_fb_pitch;

#define FB_BASE g_fb_base
#define PITCH g_fb_pitch

#define BPP 32
#define MEMORY_MODEL_RGB 6
#define LINEAR_FRAMEBUFFER (1 << 7)
#define USE_LFB (1 << 14)

void fb_initialize(uint16_t target_width, uint16_t target_height) {
    vesa_vbe_info_t vbe_info;
    *((uint8_t *) &vbe_info) = (uint8_t) 'V';
    *((uint8_t *) &vbe_info + 1) = (uint8_t) 'B';
    *((uint8_t *) &vbe_info + 2) = (uint8_t) 'E';
    *((uint8_t *) &vbe_info + 3) = (uint8_t) '2';

    int_regs_t regs = {};
    regs.eax = 0x4F00;
    regs.es = int_16bit_segment(&vbe_info);
    regs.edi = int_16bit_offset(&vbe_info);
    int_exec(0x10, &regs);
    if((regs.eax & 0xFFFF) != 0x4F) log_panic("FB", "Loading VBE 2.0 info failed");
    uint64_t closest_diff = UINT64_MAX;
    uint16_t closest_mode = 0xFFFF;

    uint16_t *modes = (uint16_t *) int_16bit_desegment(vbe_info.video_modes_segment, vbe_info.video_modes_offset);
    vesa_vbe_mode_info_t mode_info;
    while(*modes != 0xFFFF) {
        uint16_t mode = *modes;
        memset(&regs, 0, sizeof(int_regs_t));
        regs.eax = 0x4F01;
        regs.ecx = mode;
        regs.es = int_16bit_segment((uint32_t) &mode_info);
        regs.edi = int_16bit_offset((uint32_t) &mode_info);
        int_exec(0x10, &regs);
        if((regs.eax & 0xFFFF) != 0x4F) log_panic("FB", "Loading VBE mode info failed");

        modes++;

        if(mode_info.memory_model != MEMORY_MODEL_RGB) continue;
        if(mode_info.red_mask != 8 || mode_info.blue_mask != 8 || mode_info.green_mask != 8 || mode_info.reserved_mask != 8) continue;
        if(mode_info.red_position != 16 || mode_info.green_position != 8 || mode_info.blue_position != 0 || mode_info.reserved_position != 24) continue;
        if(!(mode_info.attributes & LINEAR_FRAMEBUFFER)) continue;
        if(mode_info.bpp != BPP) continue;

        int64_t signed_diff = ((int64_t) target_width - (int64_t) mode_info.width) + ((int64_t) target_height - (int64_t) mode_info.height);
        uint64_t diff = signed_diff < 0 ? -signed_diff : signed_diff;
        if(diff >= closest_diff) continue;
        closest_diff = diff;
        closest_mode = mode;
    }

    if(closest_mode == 0xFFFF) log_panic("FB", "Could not find an appropriate display mode");

    memset(&regs, 0, sizeof(int_regs_t));
    regs.eax = 0x4F01;
    regs.ecx = closest_mode;
    regs.es = int_16bit_segment((uint32_t) &mode_info);
    regs.edi = int_16bit_offset((uint32_t) &mode_info);
    int_exec(0x10, &regs);

    g_fb_width = mode_info.width;
    g_fb_height = mode_info.height;
    if(vbe_info.version_major < 3) {
        g_fb_pitch = mode_info.pitch;
    } else {
        g_fb_pitch = mode_info.linear_pitch;
    }
    g_fb_pitch /= BPP / 8;
    g_fb_base = mode_info.framebuffer;

    // tartarus_framebuffer_t *tartarus_framebuffer = (tartarus_framebuffer_t *) ((uint32_t) info_buffer + 512 + 256);
    // pmm_set(0, tartarus_framebuffer, sizeof(tartarus_framebuffer_t));
    // tartarus_framebuffer->height = mode_info->height;
    // tartarus_framebuffer->width = mode_info->width;
    // tartarus_framebuffer->bpp = mode_info->bpp;
    // tartarus_framebuffer->memory_model = mode_info->memory_model;
    // tartarus_framebuffer->address = mode_info->framebuffer;
    // if(vbe_info->version_major < 3) {
    //     tartarus_framebuffer->pitch = mode_info->pitch;
    // } else {
    //     tartarus_framebuffer->pitch = mode_info->linear_pitch;
    // }

    memset(&regs, 0, sizeof(int_regs_t));
    regs.eax = 0x4F02;
    regs.ebx = closest_mode | USE_LFB;
    int_exec(0x10, &regs);
    if((regs.eax & 0xFFFF) != 0x4F) log_panic("FB", "Setting VESA mode failed");

    g_fb_initialized = true;
    // uint64_t framebuffer_length = (uint64_t) tartarus_framebuffer->height * (uint64_t) tartarus_framebuffer->pitch;
    // if(pmm_memap_claim(tartarus_framebuffer->address, framebuffer_length, TARTARUS_MEMAP_TYPE_FRAMEBUFFER)) {
    //     log_panic("Failed to reserve framebuffer memory");
    // }
}

#endif

void fb_char(int cx, int cy, char c, uint32_t color) {
    uint8_t *font_char = &g_basicfont[c * BASICFONT_HEIGHT];
    uint64_t offset = cx + cy * PITCH;
    for(int y = 0; y < BASICFONT_HEIGHT && cy + y < (int) g_fb_height; y++) {
        for(int x = 0; x < BASICFONT_WIDTH && cx + x < (int) g_fb_width; x++) {
            if(font_char[y] & (1 << (BASICFONT_WIDTH - x))) {
                ((uint32_t *) FB_BASE)[offset + x] = color;
            }
        }
        offset += PITCH;
    }
}

void fb_clear(uint32_t color) {
    uint64_t offset = 0;
    for(int y = 0; y < g_fb_height; y++) {
        for(int x = 0; x < g_fb_width; x++) {
            ((uint32_t *) FB_BASE)[offset + x] = color;
        }
        offset += PITCH;
    }
}