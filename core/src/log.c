#include "log.h"
#include <stdint.h>
#include <stdarg.h>
#include <core.h>
#include <graphics/basicfont.h>
#if defined __BIOS && defined __AMD64
#include <int.h>
#endif

#define INSET 100
#define FG 0xFFFFFFFF
#define BG 0x00000000

static fb_t *g_fb = 0;
static const char *g_chars = "0123456789ABCDEF";
static uint32_t g_x = 0;
static uint32_t g_y = 0;

static void numprint(uint64_t value, uint64_t radix) {
    uint64_t pw = 1;
    while(value / pw >= radix) pw *= radix;

    while(pw > 0) {
        uint8_t c = value / pw;
        log_putchar(g_chars[c]);
        value %= pw;
        pw /= radix;
    }
}

static void log_list(char *str, va_list args) {
    bool escaped = false;
    while(*str) {
        if(escaped) {
            switch(*str) {
                case 's':
                    char *s = va_arg(args, char *);
                    while(*s) log_putchar(*s++);
                    break;
                case 'x':
                    log_putchar('0');
                    log_putchar('x');
                    numprint(va_arg(args, uint64_t), 16);
                    break;
                case 'i':
                    numprint(va_arg(args, uint64_t), 10);
                    break;
                default:
                    escaped = false;
                    log_putchar(*str);
                    break;
            }
        } else if(*str == '%') {
            escaped = true;
        } else {
            log_putchar(*str);
        }
        str++;
    }
}

void log_set_fb(fb_t *fb) {
    g_fb = fb;
    g_x = 0;
    g_y = 0;
    fb_clear(fb, BG);
}

void log_warning(char *src, char *str, ...) {
    log("WARN! [%s] ", src);
    va_list args;
    va_start(args, str);
    log_list(str, args);
    va_end(args);
}

[[noreturn]] void log_panic(char *src, char *str, ...) {
    log("PANIC! [%s] ", src);
    va_list args;
    va_start(args, str);
    log_list(str, args);
    va_end(args);
    while(true) asm volatile("cli\nhlt");
    __builtin_unreachable();
}

void log(char *str, ...) {
    va_list args;
    va_start(args, str);
    log_list(str, args);
    va_end(args);
}

void log_putchar(char c) {
    if(g_fb) {
        switch(c) {
            case '\n':
                g_y++;
                g_x = 0;
                break;
            case '\t':
                g_x += 4 - g_x % 4;
                break;
            default:
                fb_char(g_fb, INSET + g_x * BASICFONT_WIDTH, INSET + g_y * BASICFONT_HEIGHT, c, FG);
                g_x ++;
                break;
        }
        if(INSET + g_x * BASICFONT_WIDTH >= g_fb->width - INSET) g_x = 0;
        if(INSET + g_y * BASICFONT_HEIGHT >= g_fb->height - INSET) {
            fb_clear(g_fb, BG);
            g_y = 0;
        }
    } else {
#if defined __BIOS && defined __AMD64
        int_regs_t regs = { .eax = (0xE << 8) | c };
        int_exec(0x10, &regs);
        if(c == '\n') log_putchar('\r');
#elif defined __UEFI
        CHAR16 tmp[2] = { c, 0 };
        g_st->ConOut->OutputString(g_st->ConOut, (CHAR16 *) &tmp);
#else
#error Invalid target or missing implementation
#endif
    }
}