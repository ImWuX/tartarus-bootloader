#pragma once
#include <stdint.h>

typedef struct {
    uintptr_t address;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
    uint8_t mask_red_size;
    uint8_t mask_red_shift;
    uint8_t mask_green_size;
    uint8_t mask_green_shift;
    uint8_t mask_blue_size;
    uint8_t mask_blue_shift;
} fb_t;

bool fb_acquire(uint32_t target_width, uint32_t target_height, bool strict_rgb, fb_t *out);