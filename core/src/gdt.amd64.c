#include "gdt.h"

static gdt_entry_t gdt[] = {
    // NULL
    {},
    // Code16
    {
        .low_limit = 0xFFFF,
        .access_byte = 0b10011010,
        .flags_upper_limit = 0b00001111
    },
    // Data16
    {
        .low_limit = 0xFFFF,
        .access_byte = 0b10010010,
        .flags_upper_limit = 0b00001111
    },
    // Code32
    {
        .low_limit = 0xFFFF,
        .access_byte = 0b10011010,
        .flags_upper_limit = 0b11001111
    },
    // Data32
    {
        .low_limit = 0xFFFF,
        .access_byte = 0b10010010,
        .flags_upper_limit = 0b11001111
    },
    // Code64
    {
        .access_byte = 0b10011010,
        .flags_upper_limit = 0b00100000
    },
    // Data64
    {
        .access_byte = 0b10010010
    }
};

#ifdef __BIOS
__attribute__((section(".early")))
#endif
gdt_descriptor_t g_gdtr = {
    .limit = sizeof(gdt) - 1,
    .base = (uintptr_t) gdt
};