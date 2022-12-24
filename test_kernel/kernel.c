#include "font.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <tartarus.h>

#define HHDM 0xFFFF800000000000

static int x = 0;
static int y = 0;
static uint32_t *buf;
static uint16_t pitch;

static void putchar(char c) {
    switch(c) {
        case '\n':
            x = 0;
            y++;
            break;
        default:
            uint8_t *font_char = &font[c * 16];
            int offset = x * FONT_WIDTH + y * FONT_HEIGHT * pitch;
            for(int xx = 0; xx < FONT_HEIGHT; xx++) {
                for(int yy = 0; yy < FONT_WIDTH; yy++) {
                    if(font_char[xx] & (1 << (FONT_WIDTH - yy))){
                        buf[offset + yy] = 0xFFFFFFFF;
                    } else {
                        buf[offset + yy] = 0;
                    }
                }
                offset += pitch;
            }
            x++;
            break;
    }
}

static void pn(uint64_t value) {
    uint64_t pw = 1;
    while(value / pw >= 16) pw *= 16;

    putchar('0');
    putchar('x');
    while(pw > 0) {
        uint8_t c = value / pw;
        if(c >= 10) {
            putchar(c - 10 + 'a');
        } else {
            putchar(c + '0');
        }
        value %= pw;
        pw /= 16;
    }
}

static void ps(char *str) {
    while(*str) {
        putchar(*str);
        str++;
    }
}

noreturn void kmain(tartarus_parameters_t *params) {
    buf = (uint32_t *) params->framebuffer->address;
    pitch = params->framebuffer->pitch / 4;
    ps("Test Kernel | Initialized\n");
    ps("Test Kernel | Printing Tartarus Memap\n");
    for(uint16_t i = 0; i < params->memory_map_length; i++) {
        pn((uint64_t) i);
        ps(" >> T[");
        pn((uint64_t) params->memory_map[i].type);
        ps("] B[");
        pn((uint64_t) params->memory_map[i].base_address);
        ps("] L[");
        pn((uint64_t) params->memory_map[i].length);
        ps("]\n");
    }

    ps("Test Kernel | Performing Memory Map Read Test\n");
    for(uint16_t i = 0; i < params->memory_map_length; i++) {
        if(params->memory_map[i].type == TARTARUS_MEMAP_TYPE_USABLE) {
            uint8_t read_test = *((uint8_t *) (params->memory_map[i].base_address));
            read_test = *((uint8_t *) (params->memory_map[i].base_address + params->memory_map[i].length - 1));
            read_test = *((uint8_t *) (params->memory_map[i].base_address + params->memory_map[i].length / 2));

            read_test = *((uint8_t *) (HHDM + params->memory_map[i].base_address));
            read_test = *((uint8_t *) (HHDM + params->memory_map[i].base_address + params->memory_map[i].length - 1));
            read_test = *((uint8_t *) (HHDM + params->memory_map[i].base_address + params->memory_map[i].length / 2));
        }
    }
    ps("Test Kernel | Test Successful\n");

    while(true) asm("hlt");
}