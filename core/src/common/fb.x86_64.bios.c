#include "fb.h"
#include <lib/mem.h>
#include <sys/vesa.x86_64.bios.h>
#include <sys/int.x86_64.bios.h>

#define MEMORY_MODEL_RGB 6
#define LINEAR_FRAMEBUFFER (1 << 7)
#define USE_LFB (1 << 14)

inline static bool get_mode_info(uint16_t mode, vesa_vbe_mode_info_t *dest) {
    int_regs_t regs = {
        .eax = 0x4F01,
        .ecx = mode,
        .es = INT_16BIT_SEGMENT(dest),
        .edi = INT_16BIT_OFFSET(dest)
    };
    int_exec(0x10, &regs);
    return (regs.eax & 0xFFFF) != 0x4F;
}

bool fb_acquire(uint32_t target_width, uint32_t target_height, bool strict_rgb, fb_t *out) {
    vesa_vbe_info_t vbe_info = {};
    ((uint8_t *) &vbe_info)[0] = (uint8_t) 'V';
    ((uint8_t *) &vbe_info)[1] = (uint8_t) 'B';
    ((uint8_t *) &vbe_info)[2] = (uint8_t) 'E';
    ((uint8_t *) &vbe_info)[3] = (uint8_t) '2';

    int_regs_t regs = {
        .eax = 0x4F00,
        .es = INT_16BIT_SEGMENT(&vbe_info),
        .edi = INT_16BIT_OFFSET(&vbe_info)
    };
    int_exec(0x10, &regs);
    if((regs.eax & 0xFFFF) != 0x4F) return true;
    bool closest_found = false;
    uint16_t closest = 0;
    uint64_t closest_diff = UINT64_MAX;

    uint16_t *modes = (uint16_t *) INT_16BIT_DESEGMENT(vbe_info.video_modes_segment, vbe_info.video_modes_offset);
    vesa_vbe_mode_info_t mode_info;
    while(*modes != 0xFFFF) {
        uint16_t mode = *modes++;
        if(get_mode_info(mode, &mode_info)) continue;
        if(mode_info.memory_model != MEMORY_MODEL_RGB) continue;
        if(!(mode_info.attributes & LINEAR_FRAMEBUFFER)) continue;

        bool is_strict_rgb = false;
        if(
            mode_info.red_mask == 8 && mode_info.blue_mask == 8 && mode_info.green_mask == 8 && mode_info.reserved_mask == 8 &&
            mode_info.red_position == 16 && mode_info.green_position == 8 && mode_info.blue_position == 0 && mode_info.reserved_position == 24 &&
            mode_info.bpp == 32
        ) is_strict_rgb = true;
        if(strict_rgb && !is_strict_rgb) continue;

        int64_t signed_diff = ((int64_t) target_width - (int64_t) mode_info.width) + ((int64_t) target_height - (int64_t) mode_info.height);
        uint64_t diff = signed_diff < 0 ? -signed_diff : signed_diff;
        if(diff > closest_diff || (diff == closest_diff && !is_strict_rgb)) continue;
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
        out->mask_red_size = mode_info.red_mask;
        out->mask_red_position = mode_info.red_position;
        out->mask_green_size = mode_info.green_mask;
        out->mask_green_position = mode_info.green_position;
        out->mask_blue_size = mode_info.blue_mask;
        out->mask_blue_position = mode_info.blue_position;
    } else {
        out->pitch = mode_info.linear_pitch;
        out->mask_red_size = mode_info.linear_red_mask_size;
        out->mask_red_position = mode_info.linear_red_mask_position;
        out->mask_green_size = mode_info.linear_green_mask_size;
        out->mask_green_position = mode_info.linear_green_mask_position;
        out->mask_blue_size = mode_info.linear_blue_mask_size;
        out->mask_blue_position = mode_info.linear_blue_mask_position;
    }
    out->bpp = mode_info.bpp;
    out->width = mode_info.width;
    out->height = mode_info.height;
    out->size = out->height * out->pitch;
    return false;
}