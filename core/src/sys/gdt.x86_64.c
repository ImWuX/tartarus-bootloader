#include "gdt.x86_64.h"

static gdt_entry_t gdt[] = {
    {},
    { // code 16
        .low_limit = 0xFFFF,
        .access_byte = 0b10011010,
        .flags_upper_limit = 0b00001111
    },
    { // data 16
        .low_limit = 0xFFFF,
        .access_byte = 0b10010010,
        .flags_upper_limit = 0b00001111
    },
    { // code 32
        .low_limit = 0xFFFF,
        .access_byte = 0b10011010,
        .flags_upper_limit = 0b11001111
    },
    { // data 32
        .low_limit = 0xFFFF,
        .access_byte = 0b10010010,
        .flags_upper_limit = 0b11001111
    },
    { // code 64
        .access_byte = 0b10011010,
        .flags_upper_limit = 0b00100000
    },
    { // data 64
        .access_byte = 0b10010010
    }
};

__attribute__((section(".early"))) gdt_descriptor_t g_gdtr = {
    .limit = sizeof(gdt) - 1,
    .base = (uintptr_t) gdt
};