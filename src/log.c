#include "log.h"
#include <stdarg.h>
#include <stdbool.h>
#include <pmm.h>

#define VGA_TEXT_ADDRESS 0xB8000
#define VGA_TEXT_WIDTH 80
#define VGA_TEXT_HEIGHT 25
#define VGA_TEXT_WHITE_ON_BLACK 0x0F

static uint8_t g_x = 0;
static uint8_t g_y = 0;

static void setcursor(uint8_t x, uint8_t y) {
    if(x >= VGA_TEXT_WIDTH) {
        y++;
        x = 0;
    }
    if(y >= VGA_TEXT_HEIGHT) {
        pmm_cpy((void *) (VGA_TEXT_ADDRESS + VGA_TEXT_WIDTH * 2), (void *) VGA_TEXT_ADDRESS, VGA_TEXT_HEIGHT * VGA_TEXT_WIDTH * 2);
        pmm_set(0, (void *) (VGA_TEXT_ADDRESS + (VGA_TEXT_HEIGHT - 1) * VGA_TEXT_WIDTH * 2), VGA_TEXT_WIDTH * 2);
        y = VGA_TEXT_HEIGHT - 1;
    }
    g_x = x;
    g_y = y;
}

static void putchar(char c) {
    uint8_t *buf = (uint8_t *) VGA_TEXT_ADDRESS;
    buf[(VGA_TEXT_WIDTH * g_y + g_x) * 2] = c;
    buf[(VGA_TEXT_WIDTH * g_y + g_x) * 2 + 1] = VGA_TEXT_WHITE_ON_BLACK;
    setcursor(g_x + 1, g_y);
}

void log(char *str, ...) {
    va_list args;
    va_start(args, str);

    bool escaped = false;
    while(*str) {
        switch(*str) {
            case '\\':
                escaped = true;
                break;
            case '\n':
                setcursor(0, g_y + 1);
                break;
            case '$':
                if(escaped) {
                    putchar('$');
                } else {
                    uint64_t value = va_arg(args, uint64_t);
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
                break;
            default:
                putchar(*str);
                break;
        }
        str++;
    }

    va_end(args);
}

void log_panic(char *str) {
    log("Tartarus Panic | ");
    log(str);
    asm("cli");
    asm("hlt");
    __builtin_unreachable();
}

void log_clear() {
    uint16_t *buf = (uint16_t *) VGA_TEXT_ADDRESS;
    for(int i = 0; i < VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT; i++) buf[i] = 0;
}