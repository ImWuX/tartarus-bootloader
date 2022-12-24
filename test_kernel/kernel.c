#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <tartarus.h>

#define HHDM 0xFFFF800000000000

static int x = 0;
static int y = 8;

static void putchar(char c) {
    switch(c) {
        case '\n':
            x = 0;
            y++;
            break;
        default:
            uint8_t *buf = (uint8_t *) 0xB8000;
            buf[y * 160 + x * 2] = c;
            buf[y * 160 + x * 2 + 1] = 0x0A;
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
    ps("Test Kernel | Initialized\n");
    ps("Test Kernel | Printing Tartarus Memap\n");
    for(uint16_t i = 0; i < params->memory_map_length; i++) {
        ps("T[");
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
    ps("Test Kernel | Read Test Successful\n");

    // uint32_t *buffer = (uint32_t *) params->framebuffer->address;
    // for(int i = 0; i < 1920; i++) {
    //     buffer[i] = 0xFFFFFFFF;
    // }

    while(true) asm("hlt");
}