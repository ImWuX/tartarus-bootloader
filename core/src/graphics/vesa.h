#ifndef VESA_H
#define VESA_H

#include <stdint.h>

typedef struct {
	uint8_t signature[4];
	uint8_t version_minor;
	uint8_t version_major;
	uint16_t oem_offset;
	uint16_t oem_segment;
	uint32_t capabilities;
	uint16_t video_modes_offset;
	uint16_t video_modes_segment;
	uint16_t video_memory_blocks;
	uint16_t software_revision;
	uint16_t vendor_offset;
	uint16_t vendor_segment;
	uint16_t product_name_offset;
	uint16_t product_name_segment;
	uint16_t product_revision_offset;
	uint16_t product_revision_segment;
	uint8_t rsv0[222];
	uint8_t oem_data[256];
} __attribute__((packed)) vesa_vbe_info_t;

typedef struct {
	uint16_t attributes;
	uint8_t window_a;
	uint8_t window_b;
	uint16_t granularity;
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;
	uint16_t pitch;
	uint16_t width;
	uint16_t height;
	uint8_t w_char;
	uint8_t y_char;
	uint8_t planes;
	uint8_t bpp;
	uint8_t banks;
	uint8_t memory_model;
	uint8_t bank_size;
	uint8_t image_pages;
	uint8_t rsv0;
	uint8_t red_mask;
	uint8_t red_position;
	uint8_t green_mask;
	uint8_t green_position;
	uint8_t blue_mask;
	uint8_t blue_position;
	uint8_t reserved_mask;
	uint8_t reserved_position;
	uint8_t direct_color_attributes;
	uint32_t framebuffer;
	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;
    uint16_t linear_pitch;
    uint8_t number_of_images_banked;
    uint8_t linear_number_of_images;
    uint8_t linear_red_mask_size;
    uint8_t linear_red_mask_position;
    uint8_t linear_green_mask_size;
    uint8_t linear_green_mask_position;
    uint8_t linear_blue_mask_size;
    uint8_t linear_blue_mask_position;
    uint8_t linear_reserved_mask_size;
    uint8_t linear_reserved_mask_position;
    uint32_t maximum_pixel_clock;
    uint8_t rsv1[190];
} __attribute__ ((packed)) vesa_vbe_mode_info_t;

#endif