#include <stdint.h>
#include <tartarus.h>

static char *chars = "0123456789ABCDEF";

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a" (value), "Nd" (port));
}

void putchar(char c) {
    outb(0x3F8, (uint8_t) c);
}

void print_number(uint64_t value, uint64_t radix) {
    uint64_t pw = 1;
    while(value / pw >= radix) pw *= radix;

    while(pw > 0) {
        uint8_t c = value / pw;
        putchar(chars[c]);
        value %= pw;
        pw /= radix;
    }
}

void print(const char *str) {
    while(*str) putchar(*str++);
}

void kmain(tartarus_boot_info_t *boot_info) {
    print("Hello World\n    Kernel Paddr: ");
    print_number(boot_info->kernel_image.paddr, 16);
    print("\n    Kernel Vaddr: ");
    print_number(boot_info->kernel_image.vaddr, 16);
    print("\n    Kernel Entry: ");
    print_number(boot_info->kernel_image.entry, 16);
    print("\n    Kernel Size: ");
    print_number(boot_info->kernel_image.size, 16);
    print("\n    ACPI RSDP: ");
    print_number(boot_info->acpi_rsdp, 16);
    print("\n    FB ADDR: ");
    print_number(boot_info->framebuffer.address, 16);
    print("\n    FB SIZE: ");
    print_number(boot_info->framebuffer.size, 16);
    print("\n    FB WIDTH: ");
    print_number(boot_info->framebuffer.width, 10);
    print("\n    FB HEIGHT: ");
    print_number(boot_info->framebuffer.height, 10);
    print("\n    FB PITCH: ");
    print_number(boot_info->framebuffer.pitch, 10);
    print("\n    MEMAP: ");
    print_number(boot_info->memory_map, 16);
    print("\n    MEMAP SIZE: ");
    print_number(boot_info->memory_map_size, 10);

    while(true);
}