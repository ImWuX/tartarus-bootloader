#include <stdint.h>

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a" (value), "Nd" (port));
}

void kmain() {
    outb(0x3F8, 'H');
    outb(0x3F8, 'e');
    outb(0x3F8, 'l');
    outb(0x3F8, 'l');
    outb(0x3F8, 'o');
    outb(0x3F8, ' ');
    outb(0x3F8, 'W');
    outb(0x3F8, 'o');
    outb(0x3F8, 'r');
    outb(0x3F8, 'l');
    outb(0x3F8, 'd');

    while(true);
}