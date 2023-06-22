#ifndef GRAPHICS_FB_H
#define GRAPHICS_FB_H

#include <stdint.h>

typedef struct {
    uintptr_t address;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} fb_t;

bool fb_aquire(uint32_t target_width, uint32_t target_height, fb_t *out);
void fb_char(fb_t *fb, int x, int y, char c, uint32_t color);
void fb_clear(fb_t *fb, uint32_t color);

#endif