#include "vesa.h"
#include <tartarus.h>
#include <pmm.h>
#include <vmm.h>
#include <int.h>
#include <log.h>

#define MEMORY_MODEL_RGB 6
#define LINEAR_FRAMEBUFFER (1 << 7)
#define USE_LFB (1 << 14)

static void set_video_mode(uint16_t mode) {
    int_regs_t regs;
    pmm_set(0, &regs, sizeof(int_regs_t));
    regs.eax = 0x4F02;
    regs.ebx = mode | USE_LFB;
    int_exec(0x10, &regs);
    if(regs.eax != 0x4F) {
        log_panic("Setting VESA mode failed");
        __builtin_unreachable();
    }
}

void *vesa_setup(uint16_t target_width, uint16_t target_height, uint8_t target_bpp) {
    void *info_buffer = pmm_request_page();
    *((uint8_t *) info_buffer) = (uint8_t) 'V';
    *((uint8_t *) info_buffer + 1) = (uint8_t) 'B';
    *((uint8_t *) info_buffer + 2) = (uint8_t) 'E';
    *((uint8_t *) info_buffer + 3) = (uint8_t) '2';

    int_regs_t regs;
    pmm_set(0, &regs, sizeof(int_regs_t));
    regs.eax = 0x4F00;
    regs.es = int_16bit_segment(info_buffer);
    regs.edi = int_16bit_offset(info_buffer);
    int_exec(0x10, &regs);
    if((regs.eax & 0xFFFF) != 0x4F) {
        log_panic("Loading VBE 2.0 info failed");
        __builtin_unreachable();
    }
    vesa_vbe_info_t *vbe_info = (vesa_vbe_info_t *) info_buffer;

    uint64_t closest_diff = UINT64_MAX;
    uint16_t closest_mode = 0xFFFF;

    uint16_t *modes = (uint16_t *) int_16bit_desegment(vbe_info->video_modes_segment, vbe_info->video_modes_offset);
    while(*modes != 0xFFFF) {
        uint16_t mode = *modes;
        pmm_set(0, &regs, sizeof(int_regs_t));
        regs.eax = 0x4F01;
        regs.ecx = mode;
        regs.es = int_16bit_segment((uint32_t) info_buffer + 512);
        regs.edi = int_16bit_offset((uint32_t) info_buffer + 512);
        int_exec(0x10, &regs);
        if((regs.eax & 0xFFFF) != 0x4F) {
            log_panic("Loading VBE mode info failed");
            __builtin_unreachable();
        }

        modes++;

        vesa_vbe_mode_info_t *mode_info = (vesa_vbe_mode_info_t *) ((uint32_t) info_buffer + 512);
        if(mode_info->memory_model != MEMORY_MODEL_RGB) continue;
        if(!(mode_info->attributes & LINEAR_FRAMEBUFFER)) continue;
        if(mode_info->bpp != target_bpp) continue;

        int64_t signed_diff = ((int64_t) target_width - (int64_t) mode_info->width) + ((int64_t) target_height - (int64_t) mode_info->height);
        uint64_t diff = signed_diff < 0 ? -signed_diff : signed_diff;
        if(diff >= closest_diff) continue;
        closest_diff = diff;
        closest_mode = mode;
    }

    if(closest_mode == 0xFFFF) {
        log_panic("Could not find an appropriate display mode");
        __builtin_unreachable();
    }

    set_video_mode(closest_mode);

    pmm_set(0, &regs, sizeof(int_regs_t));
    regs.eax = 0x4F01;
    regs.ecx = closest_mode;
    regs.es = int_16bit_segment((uint32_t) info_buffer + 512);
    regs.edi = int_16bit_offset((uint32_t) info_buffer + 512);
    int_exec(0x10, &regs);
    vesa_vbe_mode_info_t *mode_info = (vesa_vbe_mode_info_t *) ((uint32_t) info_buffer + 512);
    tartarus_framebuffer_t *tartarus_framebuffer = (tartarus_framebuffer_t *) ((uint32_t) info_buffer + 512 + 256);
    pmm_set(0, tartarus_framebuffer, sizeof(tartarus_framebuffer_t));
    tartarus_framebuffer->height = mode_info->height;
    tartarus_framebuffer->width = mode_info->width;
    tartarus_framebuffer->bpp = mode_info->bpp;
    tartarus_framebuffer->memory_model = mode_info->memory_model;
    tartarus_framebuffer->address = mode_info->framebuffer;
    if(vbe_info->version_major < 3) {
        tartarus_framebuffer->pitch = mode_info->pitch;
        tartarus_framebuffer->mask_red_size = mode_info->red_mask;
        tartarus_framebuffer->mask_red_shift = mode_info->red_position;
        tartarus_framebuffer->mask_green_size = mode_info->green_mask;
        tartarus_framebuffer->mask_green_shift = mode_info->green_position;
        tartarus_framebuffer->mask_blue_size = mode_info->blue_mask;
        tartarus_framebuffer->mask_blue_shift = mode_info->blue_position;
    } else {
        tartarus_framebuffer->pitch = mode_info->linear_pitch;
        tartarus_framebuffer->mask_red_size = mode_info->linear_red_mask_size;
        tartarus_framebuffer->mask_red_shift = mode_info->linear_red_mask_position;
        tartarus_framebuffer->mask_green_size = mode_info->linear_green_mask_size;
        tartarus_framebuffer->mask_green_shift = mode_info->linear_green_mask_position;
        tartarus_framebuffer->mask_blue_size = mode_info->linear_blue_mask_size;
        tartarus_framebuffer->mask_blue_shift = mode_info->linear_blue_mask_position;
    }

    uint64_t framebuffer_length = (uint64_t) tartarus_framebuffer->height * (uint64_t) tartarus_framebuffer->pitch;
    if(pmm_memap_claim(tartarus_framebuffer->address, framebuffer_length, TARTARUS_MEMAP_TYPE_FRAMEBUFFER)) {
        log_panic("Failed to reserve framebuffer memory");
        __builtin_unreachable();
    }

    return tartarus_framebuffer;
}