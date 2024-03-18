#pragma once
#include <stdint.h>

typedef struct {
    uint16_t low_limit;
    uint16_t low_base;
    uint8_t mid_base;
    uint8_t access_byte;
    uint8_t flags_upper_limit;
    uint8_t upper_base;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_descriptor_t;

extern gdt_descriptor_t g_gdtr;