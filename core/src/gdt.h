#ifndef GDT_H
#define GDT_H

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
#ifdef __UEFI64
    uint64_t base;
#else
    uint32_t base;
#endif
} __attribute__((packed)) gdt_descriptor_t;

extern gdt_descriptor_t g_gdtr;

#endif