#include "log.h"
#include <stdint.h>
#include <stdarg.h>
#include <core.h>

static char *chars = "0123456789ABCDEF";

static void numprint(uint64_t value, uint64_t radix) {
    uint64_t pw = 1;
    while(value / pw >= radix) pw *= radix;

    while(pw > 0) {
        uint8_t c = value / pw;
        log_putchar(chars[c]);
        value %= pw;
        pw /= radix;
    }
}

void log(char *str, ...) {
    va_list args;
    va_start(args, str);

    bool escaped = false;
    while(*str) {
        if(escaped) {
            switch(*str) {
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

    va_end(args);
}

void log_putchar(__attribute__((unused)) char c) {

}

// void log_panic(char *str) {
//     log("Tartarus Panic | ");
//     log(str);
//     asm("cli\nhlt");
//     __builtin_unreachable();
// }

void log_clear() {
}